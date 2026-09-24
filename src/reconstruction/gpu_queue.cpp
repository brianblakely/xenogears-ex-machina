// The libgpu request queue of the resident executable dc0b2dd7... as the
// interrupt side reaches it: LoadImage 80044894 (with its request check
// 8004463c), the enqueue 8004668c, the queue runner 8004696c (also the DMA2
// completion callback) and the operations LoadImage 800460a0, StoreImage
// 800462dc and DrawOTag 800465ec, with the timeout pair 80046efc/80046f30 and
// the interrupt-mask swap 8004b8bc. GPU and DMA2 register stores become
// HardwareWrite records; GPUSTAT and DMA2 control reads are platform inputs.
#include "xem/reconstruction/original_layout.hpp"
#include "xem/reconstruction/program.hpp"

#include <algorithm>
#include <string>

namespace xem::reconstruction {
namespace {
constexpr std::uint32_t queue_base = 0x8006be34;
constexpr std::uint32_t entry_bytes = 0x60;
constexpr std::uint32_t ready = 0x04000000;   // GPUSTAT: ready for a command word
constexpr std::uint32_t busy = 0x01000000;    // DMA2 control: transfer active
constexpr std::uint32_t load_op = 0x800460a0; // LoadImage
constexpr std::uint32_t store_op = 0x800462dc;
constexpr std::uint32_t draw_op = 0x800465ec;
constexpr std::uint32_t queue_runner = 0x8004696c;

std::uint32_t get(std::span<const std::uint8_t> bytes, std::size_t at) {
    if (at + 4 > bytes.size())
        throw field::FieldFormatError("GPU request outside its queue");
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(bytes[at + i]) << (8U * i);
    return value;
}
void put(std::span<std::uint8_t> bytes, std::size_t at, std::uint32_t value) {
    if (at + 4 > bytes.size())
        throw field::FieldFormatError("GPU request outside its queue");
    for (std::size_t i = 0; i < 4; ++i)
        bytes[at + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
[[noreturn]] void gpu_print(std::uint32_t address) {
    throw MissingDependency({"gpu_debug_print", address, {}, {}}, "symbol:printf-80019964", false,
                            "libgpu request checking and timeout messages are not reconstructed");
}
std::uint32_t pack(std::int16_t low, std::int16_t high) {
    return static_cast<std::uint16_t>(low) |
           static_cast<std::uint32_t>(static_cast<std::uint16_t>(high)) << 16U;
}
} // namespace

std::uint32_t Program::ram_word(std::uint32_t address) const {
    const auto in = [&](const resident::HeapBlock &block) -> std::optional<std::uint32_t> {
        if (address < block.address || address - block.address + 4U > block.bytes.size())
            return std::nullopt;
        return get(block.bytes, address - block.address);
    };
    const auto &read = resident.disc_read;
    for (const auto *block : {&read.ring_payload, &read.ring, &read.list})
        if (const auto value = in(*block))
            return *value;
    for (const auto &block : resident.disc_transfers)
        if (const auto value = in(block))
            return *value;
    for (const auto &block : resident.music_blocks)
        if (const auto value = in(block))
            return *value;
    throw field::FieldFormatError("Interrupt-side code reads RAM no Program value holds");
}

// 80044894: LoadImage(rect, data) through the service table.
std::int32_t Program::load_image(std::array<std::int16_t, 4> &rect, std::uint32_t address,
                                 std::uint32_t data) {
    auto &gpu = resident.gpu;
    // 8004463c: request checking prints; level 0 checks nothing.
    if (gpu.debug != 0)
        gpu_print(0x80044720);
    const auto table = [&](std::uint32_t offset) {
        if (gpu.services != 0x80056888 || offset / 4 >= gpu.functions.size())
            throw MissingDependency({"gpu_services", 0x800448c4, {}, {}}, "symbol:gpu-services",
                                    false, "Only the observed libgpu service table is recovered");
        return gpu.functions[offset / 4];
    };
    if (table(8) != 0x8004668c)
        throw MissingDependency({"gpu_enqueue", 0x800448d8, {}, {}}, "symbol:gpu-enqueue", false,
                                "The libgpu enqueue service is not 8004668c");
    return gpu_enqueue(table(0x20), rect, address, 8, data);
}

// 8004668c: run a request at once when the queue is idle (or queueing is off),
// else copy it into the queue and run the queue.
std::int32_t Program::gpu_enqueue(std::uint32_t operation, std::array<std::int16_t, 4> &rect,
                                  std::uint32_t address, std::uint32_t size,
                                  std::uint32_t argument) {
    auto &gpu = resident.gpu;
    const auto timeout_armed = [&] { // 80046efc
        gpu.deadline = resident.vsync_counter + 240;
        gpu.polls = 0;
    };
    timeout_armed();
    while (((gpu.head + 1U) & 63U) == gpu.tail) {
        // 80046f30: the timeout path prints and resets the GPU.
        if (static_cast<std::int32_t>(gpu.deadline) <
                static_cast<std::int32_t>(resident.vsync_counter) ||
            0xf0000 < static_cast<std::int32_t>(gpu.polls++))
            gpu_print(0x80046fc8);
        static_cast<void>(gpu_execute());
    }
    const auto mask_register = resident.interrupts.registers[1];
    gpu.enqueue_mask = io_latch(mask_register, 2); // 8004b8bc(0)
    io_write(mask_register, 0, 2);
    gpu.sync_pending = 1;
    const auto chcr = [&](std::uint32_t site) { return platform_read(resident.platform, site, 4); };
    if (gpu.queued == 0 ||
        (gpu.head == gpu.tail && (chcr(0x8004674c) & busy) == 0 && gpu.sync_callback == 0)) {
        while ((platform_read(resident.platform, 0x80046780, 4) & ready) == 0) {
        }
        static_cast<void>(gpu_operation(operation, address, &rect, argument));
        gpu.current = {operation, address, argument};
        io_write(mask_register, gpu.enqueue_mask, 2);
        return 0;
    }
    set_dma_callback(2, queue_runner);
    const auto entry = gpu.head * entry_bytes;
    std::span<std::uint8_t> queue(gpu.queue);
    if (size != 0) {
        const std::array<std::uint32_t, 2> words{pack(rect[0], rect[1]), pack(rect[2], rect[3])};
        for (std::uint32_t i = 0; i < (size >> 2U); ++i)
            put(queue, entry + 0xc + 4 * i, words.at(i));
        put(queue, entry + 4, queue_base + entry + 0xc);
    } else {
        throw MissingDependency({"gpu_enqueue", 0x80046894, {}, {}}, "symbol:gpu-uncopied-request",
                                false, "Requests without a copied parameter are not recovered");
    }
    put(queue, entry + 8, argument);
    put(queue, entry, operation);
    gpu.head = (gpu.head + 1U) & 63U;
    io_write(mask_register, gpu.enqueue_mask, 2);
    static_cast<void>(gpu_execute());
    return static_cast<std::int32_t>((gpu.head - gpu.tail) & 63U);
}

void Program::gpu_wait_ready(std::uint32_t first, std::uint32_t again) {
    if ((platform_read(resident.platform, first, 4) & ready) == 0)
        while ((platform_read(resident.platform, again, 4) & ready) == 0) {
        }
}

// 8004696c: run queued requests while DMA2 is idle; when the queue empties,
// run the DrawSync callback once.
std::uint32_t Program::gpu_execute() {
    auto &gpu = resident.gpu;
    const auto chcr = [&](std::uint32_t site) {
        return (platform_read(resident.platform, site, 4) & busy) != 0;
    };
    if (chcr(0x80046980))
        return 1;
    const auto mask_register = resident.interrupts.registers[1];
    gpu.execute_mask = io_latch(mask_register, 2);
    io_write(mask_register, 0, 2);
    std::span<std::uint8_t> queue(gpu.queue);
    if (gpu.head != gpu.tail && !chcr(0x800469c8)) {
        do {
            if (((gpu.tail + 1U) & 63U) == gpu.head && gpu.sync_callback == 0)
                set_dma_callback(2, 0);
            gpu_wait_ready(0x80046a2c, 0x80046a44);
            const auto entry = gpu.tail * entry_bytes;
            const auto operation = get(queue, entry);
            const auto parameter = get(queue, entry + 4);
            const auto argument = get(queue, entry + 8);
            static_cast<void>(gpu_operation(operation, parameter, nullptr, argument));
            gpu.current = {get(queue, entry), get(queue, entry + 4), get(queue, entry + 8)};
            gpu.tail = (gpu.tail + 1U) & 63U;
        } while (gpu.head != gpu.tail && !chcr(0x80046b90));
    }
    io_write(mask_register, gpu.execute_mask, 2);
    if (gpu.head == gpu.tail && !chcr(0x80046bdc) && gpu.sync_pending != 0 &&
        gpu.sync_callback != 0)
        throw MissingDependency({"gpu_sync_callback", 0x80046c20, {}, {}},
                                "symbol:drawsync-callback", false,
                                "The DrawSync callback is not reconstructed");
    return (gpu.head - gpu.tail) & 63U;
}

std::int32_t Program::gpu_operation(std::uint32_t operation, std::uint32_t parameter,
                                    std::array<std::int16_t, 4> *rect, std::uint32_t argument) {
    auto &gpu = resident.gpu;
    const auto &registers = gpu.registers;
    if (operation == draw_op) { // 800465ec DrawOTag(ordering table)
        io_write(registers[1], 0x04000002, 4);
        io_write(registers[2], parameter, 4);
        io_write(registers[3], 0, 4);
        io_write(registers[4], 0x01000401, 4);
        return 0;
    }
    if (operation != load_op && operation != store_op)
        throw MissingDependency({"gpu_operation", 0x80046ac0, {}, {}},
                                "symbol:gpu-operation-" + std::to_string(operation), false,
                                "A queued libgpu operation is not reconstructed");
    // The rectangle: the caller's, or the copy in the queue.
    std::array<std::int16_t, 4> queued{};
    const bool in_queue = rect == nullptr;
    std::span<std::uint8_t> queue(gpu.queue);
    if (in_queue) {
        const auto at = parameter - queue_base;
        if (parameter < queue_base || at + 8 > queue.size())
            throw field::FieldFormatError("Queued image request parameter is outside the queue");
        const auto first = get(queue, at);
        const auto second = get(queue, at + 4);
        queued = {static_cast<std::int16_t>(first), static_cast<std::int16_t>(first >> 16U),
                  static_cast<std::int16_t>(second), static_cast<std::int16_t>(second >> 16U)};
        rect = &queued;
    }
    auto &r = *rect;
    gpu.deadline = resident.vsync_counter + 240; // 80046efc
    gpu.polls = 0;
    // Clamp the size to VRAM, in place.
    r[2] = r[2] < 0 ? 0 : std::min(r[2], gpu.width);
    r[3] = r[3] < 0 ? 0 : std::min(r[3], gpu.height);
    if (in_queue)
        put(queue, parameter - queue_base + 4, pack(r[2], r[3]));
    const auto pixels = static_cast<std::int32_t>(r[2]) * r[3] + 1;
    const auto rounded =
        pixels + static_cast<std::int32_t>(static_cast<std::uint32_t>(pixels) >> 31U);
    const auto words = rounded >> 1;
    if (words <= 0)
        return -1;
    const auto blocks = static_cast<std::uint32_t>(rounded >> 5);
    auto remainder = static_cast<std::uint32_t>(words) - blocks * 16U;
    const bool load = operation == load_op;
    const auto wait = [&](std::uint32_t first, std::uint32_t again, std::uint32_t bit) {
        if ((platform_read(resident.platform, first, 4) & bit) != 0)
            return;
        for (;;) {
            // 80046f30 timeout: prints and resets.
            if (static_cast<std::int32_t>(gpu.deadline) <
                    static_cast<std::int32_t>(resident.vsync_counter) ||
                0xf0000 < static_cast<std::int32_t>(gpu.polls++))
                gpu_print(0x80046fc8);
            if ((platform_read(resident.platform, again, 4) & bit) != 0)
                return;
        }
    };
    wait(load ? 0x8004618c : 0x800463c4, load ? 0x800461c0 : 0x800463f8, ready);
    io_write(registers[1], 0x04000000, 4);
    io_write(registers[0], 0x01000000, 4);
    io_write(registers[0], load ? 0xa0000000 : 0xc0000000, 4);
    io_write(registers[0], pack(r[0], r[1]), 4);
    io_write(registers[0], pack(r[2], r[3]), 4);
    auto data = argument;
    if (load) {
        for (; remainder != 0; --remainder, data += 4)
            io_write(registers[0], ram_word(data), 4);
    } else if (remainder != 0 || blocks != 0) {
        // StoreImage reads VRAM back through GPUREAD and DMA2: RAM contents
        // the platform provides, not recorded here.
        throw MissingDependency({"gpu_store_image", 0x800464d8, {}, {}}, "platform:vram-readback",
                                false, "VRAM read-back data is not a recorded platform input");
    }
    if (blocks != 0) {
        io_write(registers[1], load ? 0x04000002 : 0x04000003, 4);
        io_write(registers[2], data, 4);
        io_write(registers[3], blocks << 16U | 0x10U, 4);
        io_write(registers[4], load ? 0x01000201 : 0x01000200, 4);
    }
    return 0;
}

void add_gpu_globals(std::vector<OriginalGlobal> &table) {
    const auto add = [&](std::string name, std::uint32_t address, std::size_t width, auto access) {
        table.push_back(
            {std::move(name), address, width, true,
             [access](const Program &program) {
                 return static_cast<std::uint32_t>(
                     static_cast<std::make_unsigned_t<
                         std::remove_cvref_t<decltype(access(const_cast<Program &>(program)))>>>(
                         access(const_cast<Program &>(program))));
             },
             [access](Program &program, std::uint32_t raw) {
                 auto &value = access(program);
                 using T = std::remove_reference_t<decltype(value)>;
                 value = static_cast<T>(static_cast<std::make_unsigned_t<T>>(raw));
             }});
    };
    add("gpu_services", 0x800568c8, 4,
        [](Program &p) -> auto & { return p.resident.gpu.services; });
    for (std::uint32_t i = 0; i < 12; ++i)
        add("gpu_functions", 0x80056888 + 4 * i, 4,
            [i](Program &p) -> auto & { return p.resident.gpu.functions[i]; });
    add("gpu_queued", 0x800568d1, 1, [](Program &p) -> auto & { return p.resident.gpu.queued; });
    add("gpu_debug", 0x800568d2, 1, [](Program &p) -> auto & { return p.resident.gpu.debug; });
    add("gpu_width", 0x800568d4, 2, [](Program &p) -> auto & { return p.resident.gpu.width; });
    add("gpu_height", 0x800568d6, 2, [](Program &p) -> auto & { return p.resident.gpu.height; });
    add("gpu_sync_pending", 0x800568d8, 4,
        [](Program &p) -> auto & { return p.resident.gpu.sync_pending; });
    add("gpu_sync_callback", 0x800568dc, 4,
        [](Program &p) -> auto & { return p.resident.gpu.sync_callback; });
    for (std::uint32_t i = 0; i < 5; ++i)
        add("gpu_registers", 0x800569a0 + 4 * i, 4,
            [i](Program &p) -> auto & { return p.resident.gpu.registers[i]; });
    for (std::uint32_t i = 0; i < 3; ++i)
        add("gpu_current", 0x800569c4 + 4 * i, 4,
            [i](Program &p) -> auto & { return p.resident.gpu.current[i]; });
    add("gpu_head", 0x800569d4, 4, [](Program &p) -> auto & { return p.resident.gpu.head; });
    add("gpu_tail", 0x800569d8, 4, [](Program &p) -> auto & { return p.resident.gpu.tail; });
    add("gpu_enqueue_mask", 0x800569dc, 4,
        [](Program &p) -> auto & { return p.resident.gpu.enqueue_mask; });
    add("gpu_execute_mask", 0x800569e0, 4,
        [](Program &p) -> auto & { return p.resident.gpu.execute_mask; });
    add("gpu_deadline", 0x800569e8, 4,
        [](Program &p) -> auto & { return p.resident.gpu.deadline; });
    add("gpu_polls", 0x800569ec, 4, [](Program &p) -> auto & { return p.resident.gpu.polls; });
    // The request queue, one entry per byte.
    for (std::uint32_t i = 0; i < 64 * entry_bytes; ++i)
        add("gpu_queue", queue_base + i, 1,
            [i](Program &p) -> auto & { return p.resident.gpu.queue[i]; });
    for (std::uint32_t i = 0; i < 12; ++i)
        add("disc_image", 0x80059f24 + 4 * i, 4,
            [i](Program &p) -> auto & { return p.resident.disc_read.image[i]; });
}

} // namespace xem::reconstruction
