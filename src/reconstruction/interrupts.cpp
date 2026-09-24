// Interrupt context of the resident executable dc0b2dd7...: the dispatcher
// 8004b9b4 (entered from the BIOS exception hook, left through
// ReturnFromException), the VSync handler 8004bf78 and its per-VSync callback
// 8003634c (controllers 800358bc, input queue 80035c0c, play clock 80035e44,
// actuators 80036220), the DMA handler 8004c098 and the SPU handler 8003bfa0.
// The CD handler 80042ca8 and the disc callbacks are in disc_read.cpp.
//
// Register stores become HardwareWrite records. Registers that only software
// changes (I_MASK) read their last write or the observed I/O page; registers
// hardware changes (I_STAT, DICR flags) are platform inputs consumed in
// program order.
#include "xem/reconstruction/original_layout.hpp"
#include "xem/reconstruction/program.hpp"

#include <bit>
#include <string>

namespace xem::reconstruction {
namespace {
std::string hex(std::uint32_t value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string text(8, '0');
    for (int i = 7; i >= 0; --i, value >>= 4)
        text[static_cast<std::size_t>(i)] = digits[value & 15U];
    return text;
}
std::uint32_t io_offset(std::uint32_t address, std::uint32_t width) {
    const auto offset = address - 0x1f801000U;
    if (offset >= 0x1000U || 0x1000U - offset < width || address % width != 0)
        throw field::FieldFormatError("Hardware register outside the observed I/O page");
    return offset;
}
[[noreturn]] void unknown(const char *operation, std::uint32_t address, std::uint32_t target) {
    throw MissingDependency({operation, address, {}, {}}, "symbol:" + hex(target), false,
                            "Interrupt-context code calls a function that is not reconstructed");
}
[[noreturn]] void debug_print(std::uint32_t address) {
    throw MissingDependency({"interrupt_debug_print", address, {}, {}}, "symbol:printf-80019964",
                            false, "Interrupt diagnostics (80019964) are not reconstructed");
}
} // namespace

std::uint32_t platform_read(std::deque<PlatformInput> &inputs, std::uint32_t site,
                            std::uint32_t width, bool sign_extended) {
    if (inputs.empty() || inputs.front().kind != PlatformInput::Kind::read ||
        inputs.front().site != site)
        throw PlatformInputError("Platform input does not supply the hardware read at " +
                                 hex(site) + "; next is " +
                                 (inputs.empty() ? std::string("none")
                                  : inputs.front().kind == PlatformInput::Kind::interrupt
                                      ? std::string("an interrupt arrival")
                                      : "the read at " + hex(inputs.front().site)));
    const auto value = inputs.front().value;
    if (width < 4) {
        const auto bits = 8U * width;
        const auto low = value & ((1U << bits) - 1U);
        const auto sign = (low >> (bits - 1U)) & 1U;
        const auto extended = sign_extended && sign != 0 ? low | ~((1U << bits) - 1U) : low;
        if (value != extended)
            throw PlatformInputError("Platform read at " + hex(site) +
                                     " does not fit its load width");
    }
    inputs.pop_front();
    return value;
}

void DiscDrive::command(std::uint8_t code, std::span<const std::uint8_t> parameters) {
    const auto bcd = [](std::uint8_t value) {
        return static_cast<std::uint32_t>((value >> 4U) * 10U + (value & 15U));
    };
    switch (code) {
    case 0x02: // Setloc: minute, second, sector (BCD) of the next read.
        if (parameters.size() < 3)
            throw PlatformInputError("Setloc without its position");
        target = (bcd(parameters[0]) * 60U + bcd(parameters[1])) * 75U + bcd(parameters[2]) - 150U;
        break;
    case 0x06: // ReadN
    case 0x1b: // ReadS
        if (target) {
            next = target;
            target.reset();
        }
        reading = true;
        break;
    case 0x08: // Stop
    case 0x09: // Pause
    case 0x0a: // Init
        reading = false;
        break;
    case 0x0e: // Setmode
        if (parameters.empty())
            throw PlatformInputError("Setmode without its mode");
        mode = parameters[0];
        break;
    default: // Status, seek, filter and TOC commands deliver no data.
        break;
    }
}

void DiscDrive::data_ready() {
    cursor = 0;
    if (!next) {
        // Nothing records which sector arrived: reading it fails.
        buffer.reset();
        return;
    }
    if (!read_sector)
        throw PlatformInputError("A data-ready interrupt arrives without a disc image service");
    buffer = read_sector(*next);
    cursor = 0;
    delivered.push_back(*next);
    ++*next;
}

std::vector<std::uint8_t> DiscDrive::transfer(std::uint32_t bytes) {
    if (!buffer)
        throw PlatformInputError("A sector transfer reads an empty drive buffer");
    const std::uint32_t start = (mode & 0x20U) != 0 ? 12 : 24;
    const std::uint32_t size = (mode & 0x20U) != 0 ? 0x924 : 0x800;
    if (bytes > size - cursor)
        throw PlatformInputError("A sector transfer reads past the delivered sector");
    const auto first = buffer->begin() + start + cursor;
    cursor += bytes;
    return {first, first + bytes};
}

std::uint32_t Program::io_latch(std::uint32_t address, std::uint32_t width) const {
    const auto offset = io_offset(address, width);
    for (auto write = resident.hardware_writes.rbegin(); write != resident.hardware_writes.rend();
         ++write) {
        if (write->address == address && write->width == width)
            return write->value;
        if (write->address < address + width && address < write->address + write->width)
            throw field::FieldFormatError("A register read overlaps a write of another width");
    }
    std::uint32_t value = 0;
    for (std::uint32_t i = 0; i < width; ++i)
        value |= static_cast<std::uint32_t>(resident.io[offset + i]) << (8U * i);
    return value;
}

void Program::io_write(std::uint32_t address, std::uint32_t value, std::uint32_t width) {
    static_cast<void>(io_offset(address, width));
    resident.hardware_writes.push_back({address, value, width});
}

// 8004b9b4.
void Program::interrupt_dispatch() {
    auto &irq = resident.interrupts;
    if (irq.initialized == 0)
        debug_print(0x8004ba00);
    const auto status_register = irq.registers[0];
    const auto mask_register = irq.registers[1];
    // I_STAT is a platform input; I_MASK only changes by software stores.
    const auto pending = [&](std::uint32_t site) {
        const auto status = platform_read(resident.platform, site, 2);
        return static_cast<std::uint32_t>(irq.mask) & status & io_latch(mask_register, 2);
    };
    resident.cd.interrupt_poll = 1;
    for (auto bits = pending(0x8004ba34); bits != 0; bits = pending(0x8004bac8)) {
        for (std::uint32_t bit = 0; bit < irq.handlers.size(); ++bit) {
            if ((bits & 1U) != 0) {
                io_write(status_register, ~(1U << bit) & 0xffffU, 2);
                if (irq.handlers[bit] != 0)
                    interrupt_handler(irq.handlers[bit]);
            }
            bits >>= 1U;
            if ((bits & 0xffffU) == 0)
                break;
        }
    }
    // A still-pending unmasked bit counts; 2049 in a row print and clear I_STAT.
    const auto status = platform_read(resident.platform, 0x8004baf0, 2);
    if ((status & io_latch(mask_register, 2)) == 0) {
        irq.unexpected = 0;
    } else if (static_cast<std::int32_t>(irq.unexpected++) >= 2049) {
        debug_print(0x8004bb3c);
    }
    resident.cd.interrupt_poll = 0;
}

void Program::interrupt_handler(std::uint32_t address) {
    switch (address) {
    case 0x8004bf78:
        vsync_interrupt();
        break;
    case 0x80042ca8:
        cd_interrupt();
        break;
    case 0x8004c098:
        dma_interrupt();
        break;
    case 0x8003bfa0:
        spu_interrupt();
        break;
    default:
        unknown("interrupt_handler", 0x8004ba94, address);
    }
}

// 8004bf78: count the VSync, then run each registered callback.
void Program::vsync_interrupt() {
    ++resident.vsync_counter;
    for (const auto callback : resident.interrupts.vsync_callbacks) {
        if (callback == 0)
            continue;
        if (callback != 0x8003634c)
            unknown("vsync_callback", 0x8004bfc0, callback);
        vsync_update();
    }
}

// 8003634c.
void Program::vsync_update() {
    auto &pad = resident.pad;
    ++pad.vsyncs;
    pad_update();
    // 80035c0c: queue the six input halfwords; a full queue sets the overflow flag.
    auto &queue = resident.input_queue;
    if (queue.count < 16) {
        ++queue.count;
        for (std::size_t index = 0; index < queue.ring.size(); ++index)
            queue.ring[index][queue.write & 15U] = queue.current[index];
        ++queue.write;
    } else {
        queue.overflow = 1;
    }
    // 80035e44: play clock, stopped at 100 hours.
    if (pad.clock_stopped == 0) {
        auto &clock = pad.clock;
        if (++clock[0] == 60) {
            clock[0] = 0;
            ++clock[1];
        }
        if (clock[1] == 60) {
            clock[1] = 0;
            ++clock[2];
        }
        if (clock[2] == 60) {
            clock[2] = 0;
            ++clock[3];
        }
        if (clock[3] == 100)
            pad.clock_stopped = 1;
    }
    // 80036220 / 80036188: step each port's actuator record.
    for (auto &record : pad.actuators) {
        if (record[7] != 0)
            continue;
        const auto count = static_cast<std::uint16_t>(record[4] | record[5] << 8U);
        if (count != 0) {
            record[0] = 1;
            record[1] = 0x40;
            record[2] = 1;
            record[3] = 0;
            record[6] = 1;
            const auto left = static_cast<std::uint16_t>(count - 1U);
            record[4] = static_cast<std::uint8_t>(left);
            record[5] = static_cast<std::uint8_t>(left >> 8U);
        } else if (record[6] == 1) {
            record[0] = 1;
            record[1] = 0x40;
            record[2] = 0;
            record[3] = 0;
            record[6] = 2;
        } else if (record[6] == 2) {
            record[0] = 0;
            record[6] = 0;
        }
    }
    if (pad.hook != 0)
        unknown("vsync_hook", 0x8003639c, pad.hook);
    if (pad.text_word != 0xffffffffU && pad.debugger != 0)
        throw MissingDependency({"vsync_break", 0x800363cc, {}, {}}, "symbol:debugger-break", false,
                                "The debugger break of 8003634c is not reconstructed");
}

// 800358bc: both controller ports.
void Program::pad_update() {
    auto &pad = resident.pad;
    auto &current = resident.input_queue.current;
    const auto s16 = [](std::uint32_t value) {
        return static_cast<std::uint32_t>(
            static_cast<std::int32_t>(static_cast<std::int16_t>(value)));
    };
    for (std::size_t port = 0; port < 2; ++port) {
        const auto &buffer = pad.buffers[port];
        // 8003569c: buttons of a digital (40), analog (50) or 70 controller;
        // zero for a failed or other transfer.
        std::uint32_t buttons = 0;
        pad.type = 0;
        if (buffer[0] == 0) {
            pad.type = buffer[1] & 0xf0U;
            if (pad.type == 0x40 || pad.type == 0x50 || pad.type == 0x70)
                buttons = (~static_cast<std::uint32_t>(buffer[3]) & 0xffU) |
                          ((static_cast<std::uint32_t>(buffer[2]) << 8U) ^ 0xff00U);
        }
        current[port] = static_cast<std::uint16_t>(buttons);
        // 800357c0: keep the high byte, then map each remapped bit.
        const auto raw = s16(buttons);
        auto mapped = raw & 0xff00U;
        for (std::size_t i = 0; i < pad.remap_bits.size(); ++i)
            if ((raw & pad.remap_bits[i]) != 0)
                mapped |= pad.remap_bits.at(pad.remap_index[i]);
        current[port] = static_cast<std::uint16_t>(mapped);
        auto &analog = pad.analog[port];
        if (pad.type == 0) {
            analog = {};
        } else if (pad.type == 0x50 || pad.type == 0x70) {
            if (pad.type == 0x50) {
                // 8003582c: move five button bits.
                const auto value = s16(mapped);
                auto result = value & 0xffffff61U;
                if ((value & 0x08U) != 0)
                    result |= 0x10U;
                if ((value & 0x02U) != 0)
                    result |= 0x04U;
                if ((value & 0x10U) != 0)
                    result |= 0x08U;
                if ((value & 0x80U) != 0)
                    result |= 0x02U;
                if ((value & 0x04U) != 0)
                    result |= 0x80U;
                current[port] = static_cast<std::uint16_t>(result);
            }
            analog = {buffer[4], buffer[5], buffer[6], buffer[7]};
        } else {
            const auto nibble = (mapped & 0xffffU) >> 12U;
            analog = {0, 0, pad.direction_x[nibble], pad.direction_y[nibble]};
        }
        // Newly pressed buttons, and a repeat that fires every fourth VSync
        // once a press has been held for 32.
        const auto now = static_cast<std::uint32_t>(current[port]);
        const auto pressed = (now ^ pad.held[port]) & now;
        current[2 + port] = static_cast<std::uint16_t>(pressed);
        pad.held[port] = now;
        if (pressed != 0)
            pad.repeat_delay[port] = 0;
        current[4 + port] = static_cast<std::uint16_t>(now);
        if (static_cast<std::int32_t>(pad.repeat_delay[port]) < 32) {
            ++pad.repeat_delay[port];
            current[4 + port] = static_cast<std::uint16_t>(pressed);
        } else if ((pad.vsyncs & 3U) != 0) {
            current[4 + port] = static_cast<std::uint16_t>(pressed);
        }
    }
}

// 8004c098: acknowledge each flagged channel, then run its callback.
void Program::dma_interrupt() {
    const auto control = resident.cd.dma_interrupt_register;
    const auto flags = [&](std::uint32_t site) {
        return (platform_read(resident.platform, site, 4) >> 24U) & 0x7fU;
    };
    for (auto pending = flags(0x8004c0c0); pending != 0; pending = flags(0x8004c15c)) {
        for (std::uint32_t channel = 0; channel < 7; ++channel) {
            if ((pending & 1U) != 0) {
                const auto value = platform_read(resident.platform, 0x8004c118, 4);
                io_write(control, value & ((1U << (channel + 24U)) | 0x00ffffffU), 4);
                const auto callback = channel == 3 ? resident.cd.dma_callback
                                                   : resident.interrupts.dma_callbacks[channel];
                if (callback != 0)
                    dma_completed(callback);
            }
            pending >>= 1U;
            if (pending == 0)
                break;
        }
    }
    // A master flag without channel flags, or a forced interrupt, prints.
    if ((platform_read(resident.platform, 0x8004c180, 4) & 0xff000000U) == 0x80000000U ||
        (platform_read(resident.platform, 0x8004c198, 4) & 0x8000U) != 0)
        debug_print(0x8004c1b8);
}

void Program::dma_completed(std::uint32_t address) {
    switch (address) {
    case 0x8002ba40: // Mark the read finished with the value 8004fe00 holds.
        resident.disc_error = resident.disc_read.w_fe00;
        break;
    case 0x8002ba58:
        disc_ring_transferred();
        break;
    case 0x8002bb50:
        disc_image_transferred();
        break;
    case 0x8004696c: // The libgpu queue runner.
        static_cast<void>(gpu_execute());
        break;
    default:
        unknown("dma_callback", 0x8004c138, address);
    }
}

// 8003bfa0.
void Program::spu_interrupt() {
    auto &irq = resident.interrupts;
    resident.sound.flags |= 4U;
    ++irq.spu_count;
    if (irq.spu_callback != 0)
        unknown("spu_callback", 0x8003bfe0, irq.spu_callback);
    resident.sound.flags &= static_cast<std::uint16_t>(~4U);
}

namespace {
template <typename T> T narrow(std::uint32_t raw) {
    if constexpr (std::is_signed_v<T>)
        return std::bit_cast<T>(static_cast<std::make_unsigned_t<T>>(raw));
    else
        return static_cast<T>(raw);
}
} // namespace

void add_interrupt_globals(std::vector<OriginalGlobal> &table) {
    const auto add = [&](std::string name, std::uint32_t address, std::size_t width, auto access) {
        table.push_back({std::move(name), address, width, true,
                         [access](const Program &program) {
                             return static_cast<std::uint32_t>(
                                 access(const_cast<Program &>(program)));
                         },
                         [access](Program &program, std::uint32_t raw) {
                             auto &value = access(program);
                             value = narrow<std::remove_reference_t<decltype(value)>>(raw);
                         }});
    };
    const auto each = [&](std::string name, std::uint32_t address, std::size_t width,
                          std::size_t count, auto access) {
        for (std::size_t i = 0; i < count; ++i)
            add(name, address + static_cast<std::uint32_t>(i * width), width,
                [access, i](Program &p) -> auto & { return access(p)[i]; });
    };
    // Dispatcher and handler tables.
    add("irq_initialized", 0x800578a4, 2,
        [](Program &p) -> auto & { return p.resident.interrupts.initialized; });
    each("irq_handlers", 0x800578a8, 4, 11,
         [](Program &p) -> auto & { return p.resident.interrupts.handlers; });
    add("irq_mask", 0x800578d4, 2, [](Program &p) -> auto & { return p.resident.interrupts.mask; });
    each("irq_registers", 0x80058930, 4, 3,
         [](Program &p) -> auto & { return p.resident.interrupts.registers; });
    add("irq_unexpected", 0x8005893c, 4,
        [](Program &p) -> auto & { return p.resident.interrupts.unexpected; });
    each("vsync_callbacks", 0x80058940, 4, 8,
         [](Program &p) -> auto & { return p.resident.interrupts.vsync_callbacks; });
    for (std::uint32_t i = 0; i < 7; ++i)
        if (i != 3) // 80058978 is CdState::dma_callback.
            add("dma_callbacks", 0x8005896c + 4 * i, 4,
                [i](Program &p) -> auto & { return p.resident.interrupts.dma_callbacks[i]; });
    add("irq_hook_stack", 0x800578e0, 4,
        [](Program &p) -> auto & { return p.resident.interrupts.hook_stack; });
    add_gpu_globals(table);
    add("spu_callback", 0x8005950c, 4,
        [](Program &p) -> auto & { return p.resident.interrupts.spu_callback; });
    add("spu_count", 0x80059514, 4,
        [](Program &p) -> auto & { return p.resident.interrupts.spu_count; });
    // Per-VSync controller state.
    add("pad_vsyncs", 0x80059488, 4, [](Program &p) -> auto & { return p.resident.pad.vsyncs; });
    add("pad_hook", 0x800501fc, 4, [](Program &p) -> auto & { return p.resident.pad.hook; });
    add("pad_debugger", 0x80059390, 4,
        [](Program &p) -> auto & { return p.resident.pad.debugger; });
    add("pad_type", 0x80059388, 1, [](Program &p) -> auto & { return p.resident.pad.type; });
    each("pad_held", 0x80059374, 4, 2, [](Program &p) -> auto & { return p.resident.pad.held; });
    each("pad_repeat_delay", 0x8005022c, 4, 2,
         [](Program &p) -> auto & { return p.resident.pad.repeat_delay; });
    const std::array<std::array<std::uint32_t, 4>, 2> analog{
        {{0x80059444, 0x8005944c, 0x80059430, 0x80059438},
         {0x80059448, 0x80059450, 0x80059434, 0x8005943c}}};
    for (std::size_t port = 0; port < 2; ++port)
        for (std::size_t i = 0; i < 4; ++i)
            add("pad_analog", analog[port][i], 1,
                [port, i](Program &p) -> auto & { return p.resident.pad.analog[port][i]; });
    each("pad_remap_bits", 0x800501e8, 2, 8,
         [](Program &p) -> auto & { return p.resident.pad.remap_bits; });
    each("pad_remap_index", 0x80050238, 1, 8,
         [](Program &p) -> auto & { return p.resident.pad.remap_index; });
    each("pad_direction_x", 0x8005020c, 1, 16,
         [](Program &p) -> auto & { return p.resident.pad.direction_x; });
    each("pad_direction_y", 0x8005021c, 1, 16,
         [](Program &p) -> auto & { return p.resident.pad.direction_y; });
    add("clock_stopped", 0x800501f8, 1,
        [](Program &p) -> auto & { return p.resident.pad.clock_stopped; });
    const std::array<std::uint32_t, 4> clock{0x80059370, 0x80059418, 0x80059420, 0x80059484};
    for (std::size_t i = 0; i < clock.size(); ++i)
        add("play_clock", clock[i], 1,
            [i](Program &p) -> auto & { return p.resident.pad.clock[i]; });
    for (std::size_t port = 0; port < 2; ++port)
        each("pad_actuators", 0x8005a1bc + 8 * static_cast<std::uint32_t>(port), 1, 8,
             [port](Program &p) -> auto & { return p.resident.pad.actuators[port]; });
    for (std::size_t field = 0; field < 6; ++field)
        each("input_ring", 0x8005a0fc + 0x20 * static_cast<std::uint32_t>(field), 2, 16,
             [field](Program &p) -> auto & { return p.resident.input_queue.ring[field]; });
    // CD interrupt side.
    add("cd_status2", 0x800564bc, 4, [](Program &p) -> auto & { return p.resident.cd.status2; });
    add("cd_shell_opened", 0x800564c0, 4,
        [](Program &p) -> auto & { return p.resident.cd.shell_opened; });
    add("cd_end_status", 0x8005678a, 1,
        [](Program &p) -> auto & { return p.resident.cd.end_status; });
    each("cd_sync_result", 0x8005a210, 1, 8,
         [](Program &p) -> auto & { return p.resident.cd.sync_result; });
    each("cd_ready_result", 0x8005a218, 1, 8,
         [](Program &p) -> auto & { return p.resident.cd.ready_result; });
    each("cd_end_result", 0x8005a220, 1, 8,
         [](Program &p) -> auto & { return p.resident.cd.end_result; });
    each("cd_completes", 0x80056570, 4, 32,
         [](Program &p) -> auto & { return p.resident.cd.completes; });
    each("cd_ack_updates", 0x80056670, 4, 32,
         [](Program &p) -> auto & { return p.resident.cd.ack_updates; });
    add("cd_flag_register", 0x8005677c, 4,
        [](Program &p) -> auto & { return p.resident.cd.flag_register; });
    const std::array<std::uint32_t, 5> transfer{0x800567a4, 0x80056780, 0x800567a8, 0x800567ac,
                                                0x800567b0};
    for (std::size_t i = 0; i < transfer.size(); ++i)
        add("cd_transfer_registers", transfer[i], 4,
            [i](Program &p) -> auto & { return p.resident.cd.transfer_registers[i]; });
    // Interrupt-side read state.
    const auto disc = [&](std::string name, std::uint32_t address, std::size_t width, auto member) {
        add(std::move(name), address, width,
            [member](Program &p) -> auto & { return p.resident.disc_read.*member; });
    };
    using Read = DiscReadState;
    disc("disc_retry_reason", 0x8004fe20, 4, &Read::retry_reason);
    disc("disc_fe00", 0x8004fe00, 4, &Read::w_fe00);
    disc("disc_fe3c", 0x8004fe3c, 4, &Read::w_fe3c);
    disc("disc_fde0", 0x8004fde0, 4, &Read::w_fde0);
    disc("disc_saved_callback", 0x80059f08, 4, &Read::saved_callback);
    disc("disc_5a4b4", 0x8005a4b4, 4, &Read::w_5a4b4);
    each("disc_skipped", 0x8004fde4, 4, 3,
         [](Program &p) -> auto & { return p.resident.disc_read.skipped; });
    each("disc_59f14", 0x80059f14, 1, 2,
         [](Program &p) -> auto & { return p.resident.disc_read.b_59f14; });
    each("disc_5a48c", 0x8005a48c, 4, 2,
         [](Program &p) -> auto & { return p.resident.disc_read.w_5a48c; });
    each("disc_5a494", 0x8005a494, 4, 2,
         [](Program &p) -> auto & { return p.resident.disc_read.w_5a494; });
    each("disc_5a4a4", 0x8005a4a4, 4, 2,
         [](Program &p) -> auto & { return p.resident.disc_read.w_5a4a4; });
}

} // namespace xem::reconstruction
