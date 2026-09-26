// Resident libgpu of executable dc0b2dd7...: the request queue (enqueue
// 8004668c, runner 8004696c, also the DMA2 completion callback) with its
// alarm 80046efc and request check 8004463c, and the calls that go through
// it: LoadImage 80044894 (_dws 800460a0), StoreImage (_drs 800462dc),
// ClearImage 80044764 (_clr 80045e44), DrawOTag 80044bd0 and PutDrawEnv
// 80044c44 (_cwc 800465ec), plus PutDispEnv 80044e9c and DrawSync 800445d0.
// Operations that reach the GPU or its DMA channel become GpuCommand records;
// the interrupt-mask swap 8004b8bc stores I_MASK as hardware writes.
//
// Platform results come from a field frame's services when a frame makes the
// call, else (interrupt-side calls) from the recorded hardware reads. A frame's
// services do not record GPUSTAT reads of waits with no state effect.
#include "xem/reconstruction/original_layout.hpp"
#include "xem/reconstruction/program.hpp"

#include <algorithm>
#include <bit>
#include <string>
#include <vector>

namespace xem::reconstruction {
namespace {
constexpr std::uint32_t queue_base = 0x8006be34;
constexpr std::uint32_t entry_bytes = 0x60;
constexpr std::uint32_t ready = 0x04000000;    // GPUSTAT: ready for a command word
constexpr std::uint32_t busy = 0x01000000;     // DMA2 control: transfer active
constexpr std::uint32_t load_op = 0x800460a0;  // _dws: LoadImage
constexpr std::uint32_t store_op = 0x800462dc; // _drs: StoreImage
constexpr std::uint32_t send_op = 0x800465ec;  // _cwc: DrawOTag, PutDrawEnv
constexpr std::uint32_t clear_op = 0x80045e44; // _clr: ClearImage
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
std::int16_t s16(std::uint32_t value) {
    return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}
std::uint32_t u32(std::int32_t value) { return static_cast<std::uint32_t>(value); }
// Clamp a rectangle extent to [0, limit], or [0, limit - 1] when inclusive.
std::int16_t clamp_extent(std::int16_t value, std::int16_t limit, bool inclusive) {
    const auto top = static_cast<std::int16_t>(inclusive ? limit - 1 : limit);
    if (value < 0)
        return 0;
    return value > top ? top : value;
}
} // namespace

// 80046efc: a deadline 240 vertical blanks after VSync(-1).
void Program::gpu_alarm(FrameServices *services) {
    auto &gpu = resident.gpu;
    gpu.deadline =
        (services != nullptr ? take_service(services->vblank_counts, "VSync(-1) for a libgpu alarm")
                             : resident.vsync_counter) +
        0xf0U;
    gpu.polls = 0;
}

// 8004463c: request checking prints; level 0 checks nothing.
void Program::gpu_check_rect() const {
    if (resident.gpu.debug != 0)
        gpu_print(0x80044720);
}

// 80044894: LoadImage(rect, data) through the service table.
std::int32_t Program::load_image(std::array<std::int16_t, 4> &rect, std::uint32_t address,
                                 std::uint32_t data, FrameServices *services) {
    auto &gpu = resident.gpu;
    gpu_check_rect();
    const auto table = [&](std::uint32_t offset) {
        if (gpu.services != 0x80056888 || offset / 4 >= gpu.functions.size())
            throw MissingDependency({"gpu_services", 0x800448c4, {}, {}}, "symbol:gpu-services",
                                    false, "Only the observed libgpu service table is recovered");
        return gpu.functions[offset / 4];
    };
    if (table(8) != 0x8004668c)
        throw MissingDependency({"gpu_enqueue", 0x800448d8, {}, {}}, "symbol:gpu-enqueue", false,
                                "The libgpu enqueue service is not 8004668c");
    return gpu_enqueue(table(0x20), address, &rect, 8, data, services);
}

// LoadImage of the rectangle at `rect` in owned memory, which the call clamps
// in place.
void Program::load_image_at(FrameServices &services, std::uint32_t rect, std::uint32_t source) {
    std::array<std::int16_t, 4> area{s16(memory(rect, 2)), s16(memory(rect + 2, 2)),
                                     s16(memory(rect + 4, 2)), s16(memory(rect + 6, 2))};
    static_cast<void>(load_image(area, rect, source, &services));
    set_memory(rect + 4, static_cast<std::uint16_t>(area[2]), 2);
    set_memory(rect + 6, static_cast<std::uint16_t>(area[3]), 2);
}

// 8004668c: run a request at once when the queue is idle (or queueing is off),
// else copy its rectangle into the queue and run the queue.
std::int32_t Program::gpu_enqueue(std::uint32_t operation, std::uint32_t parameter,
                                  std::array<std::int16_t, 4> *rect, std::uint32_t size,
                                  std::uint32_t argument, FrameServices *services) {
    auto &gpu = resident.gpu;
    gpu_alarm(services);
    while (((gpu.head + 1U) & 63U) == gpu.tail) {
        // 80046f30: the timeout path prints and resets the GPU.
        if (static_cast<std::int32_t>(gpu.deadline) <
                static_cast<std::int32_t>(resident.vsync_counter) ||
            0xf0000 < static_cast<std::int32_t>(gpu.polls++))
            gpu_print(0x80046fc8);
        static_cast<void>(gpu_execute());
    }
    // 8004b8bc(0): SetIntrMask returns the mask it replaces.
    const auto mask_register = resident.interrupts.registers[1];
    gpu.enqueue_mask = services != nullptr
                           ? take_service(services->interrupt_masks, "SetIntrMask(0) result")
                           : io_latch(mask_register, 2);
    io_write(mask_register, 0, 2);
    gpu.sync_pending = 1;
    const auto dma_busy = [&] {
        const auto control = services != nullptr
                                 ? take_service(services->dma_busy, "libgpu DMA busy check")
                                 : platform_read(resident.platform, 0x8004674c, 4);
        return (control & busy) != 0;
    };
    if (gpu.queued == 0 || (gpu.head == gpu.tail && !dma_busy() && gpu.sync_callback == 0)) {
        // The GPUSTAT wait (80046780) has no recorded result; the GPU is
        // taken as ready at once.
        static_cast<void>(gpu_operation(operation, parameter, rect, argument, services));
        gpu.current = {operation, parameter, argument};
        io_write(mask_register, gpu.enqueue_mask, 2);
        return 0;
    }
    set_dma_callback(2, queue_runner);
    const auto entry = gpu.head * entry_bytes;
    std::span<std::uint8_t> queue(gpu.queue);
    if (size == 0 || rect == nullptr)
        throw MissingDependency({"gpu_enqueue", 0x80046894, {}, {}}, "symbol:gpu-uncopied-request",
                                false, "Requests without a copied rectangle are not recovered");
    const std::array<std::uint32_t, 2> words{pack((*rect)[0], (*rect)[1]),
                                             pack((*rect)[2], (*rect)[3])};
    for (std::uint32_t i = 0; i < (size >> 2U); ++i)
        put(queue, entry + 0xc + 4 * i, words.at(i));
    put(queue, entry + 4, queue_base + entry + 0xc);
    put(queue, entry + 8, argument);
    put(queue, entry, operation);
    gpu.head = (gpu.head + 1U) & 63U;
    io_write(mask_register, gpu.enqueue_mask, 2);
    // An interrupt recorded before the runner's first read was taken once
    // the mask was restored.
    deliver_leading_arrivals();
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
        deliver_due_arrivals();
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
            static_cast<void>(gpu_operation(operation, parameter, nullptr, argument, nullptr));
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
                                    std::array<std::int16_t, 4> *rect, std::uint32_t argument,
                                    FrameServices *services) {
    auto &gpu = resident.gpu;
    if (operation == send_op) { // _cwc: the packet list at `parameter` by DMA
        gpu.commands.push_back({GpuCommand::Kind::draw_packets, {}, parameter, 0});
        return 0;
    }
    if (operation == clear_op) {
        clear_operation(parameter, argument, services);
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
    gpu_alarm(services);
    // Clamp the size to VRAM, in place.
    r[2] = clamp_extent(r[2], gpu.width, false);
    r[3] = clamp_extent(r[3], gpu.height, false);
    if (in_queue)
        put(queue, parameter - queue_base + 4, pack(r[2], r[3]));
    const auto pixels = static_cast<std::int32_t>(r[2]) * r[3] + 1;
    const auto rounded =
        pixels + static_cast<std::int32_t>(static_cast<std::uint32_t>(pixels) >> 31U);
    const auto words = rounded >> 1;
    if (words <= 0)
        return -1;
    const bool load = operation == load_op;
    if (!load) {
        // StoreImage (_drs) reads VRAM back through GPUREAD and DMA2 into the
        // owned destination; the words are a platform input. Its GPUSTAT
        // waits have no recorded result; the GPU is taken as ready at once,
        // leaving the alarm's poll count.
        if (gpu.vram_reads.empty())
            throw MissingDependency({"gpu_store_image", 0x800464d8, {}, {}},
                                    "platform:vram-readback", false,
                                    "VRAM read-back data is not a recorded platform input");
        auto data = std::move(gpu.vram_reads.front());
        gpu.vram_reads.pop_front();
        if (data.size() != static_cast<std::size_t>(words) * 4U)
            throw field::FieldFormatError("VRAM read-back size differs from its rectangle");
        // The destination is an owned block or Program globals.
        if (const auto destination = owned_span(argument); destination.size() >= data.size())
            std::ranges::copy(data, destination.begin());
        else
            write_original(*this, argument, data);
        gpu.commands.push_back({GpuCommand::Kind::store_image, r, argument, 0});
        return 0;
    }
    // Wait for GPUSTAT bit 26, polling the alarm.
    if (services != nullptr) {
        gpu.polls = take_service(services->alarm_polls, "LoadImage alarm polls");
    } else {
        const auto first = load ? 0x8004618cU : 0x800463c4U;
        const auto again = load ? 0x800461c0U : 0x800463f8U;
        if ((platform_read(resident.platform, first, 4) & ready) == 0)
            for (;;) {
                // 80046f30 timeout: prints and resets.
                if (static_cast<std::int32_t>(gpu.deadline) <
                        static_cast<std::int32_t>(resident.vsync_counter) ||
                    0xf0000 < static_cast<std::int32_t>(gpu.polls++))
                    gpu_print(0x80046fc8);
                if ((platform_read(resident.platform, again, 4) & ready) != 0)
                    break;
            }
    }
    gpu.commands.push_back({GpuCommand::Kind::load_image, r, argument, 0});
    return 0;
}

// _clr (80045e44): send a fill packet built in libgpu's packet buffer
// (8005a238) for the rectangle at `rect` in owned memory, clamped in place.
void Program::clear_operation(std::uint32_t rect, std::uint32_t color, FrameServices *services) {
    if (services == nullptr)
        throw MissingDependency({"gpu_clear_image", 0x80045e44, {}, {}}, "symbol:gpu-clear-image",
                                false, "ClearImage outside a field frame is not recovered");
    auto &gpu = resident.gpu;
    const auto w = clamp_extent(s16(memory(rect + 4, 2)), gpu.width, true);
    set_memory(rect + 4, static_cast<std::uint16_t>(w), 2);
    const auto h = clamp_extent(s16(memory(rect + 6, 2)), gpu.height, true);
    set_memory(rect + 6, static_cast<std::uint16_t>(h), 2);
    const auto status = take_service(services->gpu_status, "GPUSTAT read by ClearImage");
    const auto mode = 0xe1000000U | (color >> 31U) << 10U | (status & 0x7ffU);
    std::vector<std::uint32_t> packet;
    if ((memory(rect, 2) & 0x3fU) == 0 && (static_cast<std::uint32_t>(w) & 0x3fU) == 0) {
        // Aligned: a VRAM fill.
        packet = {0x05ffffffU,  0xe6000000U,     mode, 0x02000000U | (color & 0xffffffU),
                  memory(rect), memory(rect + 4)};
    } else {
        // Unaligned: open the drawing area, draw a rectangle, then restore
        // the area and offset read back through _param (80046638): GP1
        // 10000003..5, then GPUREAD.
        const auto info = [&](std::uint32_t index) {
            gpu.commands.push_back({GpuCommand::Kind::control, {}, 0, 0x10000000U | index});
            return take_service(services->gpu_info, "GPUREAD for ClearImage") & 0xffffffU;
        };
        packet = {0x0805a25cU,
                  0xe3000000U,
                  0xe4ffffffU,
                  0xe5000000U,
                  0xe6000000U,
                  mode,
                  0x60000000U | (color & 0xffffffU),
                  memory(rect),
                  memory(rect + 4),
                  0x03ffffffU};
        for (std::uint32_t index = 3; index <= 5; ++index)
            packet.push_back(info(index) | (0xe0000000U + (index << 24U)));
    }
    for (std::size_t i = 0; i < packet.size(); ++i)
        for (std::size_t b = 0; b < 4; ++b)
            gpu.packet[i * 4 + b] = static_cast<std::uint8_t>(packet[i] >> (8U * b));
    gpu.commands.push_back({GpuCommand::Kind::clear_image,
                            {s16(memory(rect, 2)), s16(memory(rect + 2, 2)), w, h},
                            0x8005a238,
                            color});
}

// ClearImage (80044764).
void Program::clear_image(FrameServices &services, std::uint32_t rect, std::uint32_t color) {
    gpu_check_rect();
    static_cast<void>(gpu_enqueue(clear_op, rect, nullptr, 8, color, &services));
}

// DrawOTag (80044bd0): send the packet list by DMA.
void Program::draw_otag(FrameServices &services, std::uint32_t table) {
    if (resident.gpu.debug >= 2)
        gpu_print(0x80044bf0);
    static_cast<void>(gpu_enqueue(send_op, table, nullptr, 0, 0, &services));
}

// PutDrawEnv (80044c44): build the environment's packet with SetDrawEnv2
// (8004574c), send it, and keep a copy of the environment (800568e0).
void Program::put_draw_env(FrameServices &services, std::uint32_t environment) {
    auto &gpu = resident.gpu;
    if (gpu.debug >= 2)
        gpu_print(0x80044c74);
    const auto type = resident.gpu_type;
    const bool low = static_cast<std::uint8_t>(type - 1U) < 2;
    const auto coordinate = [&](std::int32_t value, std::int16_t limit) {
        return clamp_extent(static_cast<std::int16_t>(value), limit, true);
    };
    // get_cs/get_ce (80045a34/80045b00): a clamped drawing-area corner.
    const auto corner = [&](std::uint32_t command, std::int32_t x, std::int32_t y) {
        const auto cx = static_cast<std::uint32_t>(coordinate(x, gpu.width));
        const auto cy = static_cast<std::uint32_t>(coordinate(y, gpu.height));
        return command |
               (low ? (cy & 0xfffU) << 12U | (cx & 0xfffU) : (cy & 0x3ffU) << 10U | (cx & 0x3ffU));
    };
    const auto e = environment;
    const auto clip_x = s16(memory(e, 2));
    const auto clip_y = s16(memory(e + 2, 2));
    const auto clip_w = s16(memory(e + 4, 2));
    const auto clip_h = s16(memory(e + 6, 2));
    const auto packet = e + 0x1c;
    set_memory(packet + 4, corner(0xe3000000U, clip_x, clip_y));
    set_memory(packet + 8,
               corner(0xe4000000U, s16(u32(clip_x + clip_w - 1)), s16(u32(clip_y + clip_h - 1))));
    // get_ofs (80045bcc).
    const auto ox = memory(e + 8, 2);
    const auto oy = memory(e + 0xa, 2);
    set_memory(packet + 0xc, 0xe5000000U | (low ? (oy & 0xfffU) << 12U | (ox & 0xfffU)
                                                : (oy & 0x7ffU) << 11U | (ox & 0x7ffU)));
    set_memory(packet + 0x10, gpu::draw_mode(type, memory(e + 0x17, 1) != 0,
                                             memory(e + 0x16, 1) != 0, memory(e + 0x14, 2)));
    const std::array<std::int16_t, 4> window{s16(memory(e + 0xc, 2)), s16(memory(e + 0xe, 2)),
                                             s16(memory(e + 0x10, 2)), s16(memory(e + 0x12, 2))};
    set_memory(packet + 0x14, gpu::texture_window(&window));
    set_memory(packet + 0x18, 0xe6000000U);
    std::uint32_t words = 6;
    if (memory(e + 0x18, 1) != 0) {
        // 80045800: clear the drawing area with the background color: a
        // VRAM fill (02) when 64-pixel aligned, else a tile (60) placed
        // relative to the drawing offset. The size is clamped to VRAM.
        const auto clamp = [](std::int32_t value, std::int16_t limit) {
            return value < 0 ? 0 : limit - 1 < value ? limit - 1 : value;
        };
        const auto w = static_cast<std::uint32_t>(clamp(clip_w, gpu.width)) & 0xffffU;
        const auto h = static_cast<std::uint32_t>(clamp(clip_h, gpu.height)) & 0xffffU;
        const auto rgb =
            memory(e + 0x1b, 1) << 16U | memory(e + 0x1a, 1) << 8U | memory(e + 0x19, 1);
        auto x = static_cast<std::uint32_t>(clip_x) & 0xffffU;
        auto y = static_cast<std::uint32_t>(clip_y) & 0xffffU;
        const bool aligned = (x & 0x3fU) == 0 && (w & 0x3fU) == 0;
        if (!aligned) {
            x = (x - memory(e + 8, 2)) & 0xffffU;
            y = (y - memory(e + 0xa, 2)) & 0xffffU;
        }
        set_memory(packet + 0x1c, (aligned ? 0x02000000U : 0x60000000U) | rgb);
        set_memory(packet + 0x20, y << 16U | x);
        set_memory(packet + 0x24, h << 16U | w);
        words = 9;
    }
    set_memory(packet + 3, words, 1);
    set_memory(packet, memory(packet) | 0xffffffU);
    static_cast<void>(gpu_enqueue(send_op, packet, nullptr, 0, 0, &services));
    for (std::uint32_t i = 0; i < gpu.draw_environment.size(); ++i)
        gpu.draw_environment[i] = static_cast<std::uint8_t>(memory(e + i, 1));
}

// SetDispMask 80044534 through libgpu _ctl (80046560): GP1 command 3 enables
// the display for a nonzero mask. A zero mask first clears libgpu's display
// environment copy (8005693c, 80047178).
void Program::set_display_mask(std::uint32_t mask) {
    auto &gpu = resident.gpu;
    if (gpu.debug >= 2)
        gpu_print(0x80044574);
    // Disabling forgets the last PutDispEnv (80047178 fills it with ff).
    if (mask == 0)
        gpu.display_environment.fill(0xff);
    if (gpu.services != 0x80056888 || gpu.functions[4] != 0x80046560)
        throw MissingDependency({"set_display_mask", 0x800445b0, {}, {}}, "symbol:gpu-services",
                                false, "Only the observed libgpu control service is recovered");
    const std::uint32_t command = mask != 0 ? 0x03000000U : 0x03000001U;
    gpu.control[command >> 24U] = static_cast<std::uint8_t>(command);
    gpu.commands.push_back({GpuCommand::Kind::control, {}, 0, command});
}

// PutDispEnv (80044e9c): display start, then display ranges and mode when
// they changed since the copy kept at 8005693c.
void Program::put_disp_env(std::uint32_t environment) {
    auto &gpu = resident.gpu;
    if (gpu.debug >= 2)
        gpu_print(0x80044ecc);
    if (static_cast<std::uint8_t>(resident.gpu_type - 1U) < 2)
        throw MissingDependency({"put_disp_env", 0x80044f04, {}, {}}, "symbol:libgpu-display-type",
                                false,
                                "Display commands for libgpu types 1 and 2 are not recovered");
    const auto e = environment;
    // _ctl (80046560): a GP1 command, remembered per command number.
    const auto control = [&](std::uint32_t command) {
        gpu.control[command >> 24U] = static_cast<std::uint8_t>(command);
        gpu.commands.push_back({GpuCommand::Kind::control, {}, 0, command});
    };
    control(0x05000000U | (memory(e + 2, 2) & 0x3ffU) << 10U | (memory(e, 2) & 0x3ffU));
    const auto previous = [&](std::uint32_t offset) {
        return static_cast<std::uint32_t>(gpu.display_environment[offset]) |
               static_cast<std::uint32_t>(gpu.display_environment[offset + 1]) << 8U;
    };
    bool screen_same = true;
    for (std::uint32_t i = 8; i < 0x10; i += 2)
        screen_same = screen_same && previous(i) == memory(e + i, 2);
    if (!screen_same) {
        // 80044fd8: the screen range as GPU display ranges (GP1 06, 07):
        // horizontal in dot clocks from 608, vertical from line 16 (19 in
        // PAL), a zero size meaning the full 2560 clocks or 240 lines, all
        // clamped to the visible area.
        set_memory(e + 0x12, resident.video_mode, 1); // GetVideoMode (8004c308)
        const bool pal = (resident.video_mode & 0xffU) != 0;
        const auto sx = s16(memory(e + 8, 2));
        const auto sy = s16(memory(e + 0xa, 2));
        const auto sw = s16(memory(e + 0xc, 2));
        const auto sh = s16(memory(e + 0xe, 2));
        auto x1 = sx * 10 + 608;
        auto y1 = sy + (pal ? 19 : 16);
        auto x2 = sw != 0 ? x1 + sw * 10 : x1 + 2560;
        auto y2 = sh != 0 ? y1 + sh : y1 + 240;
        x1 = x1 < 500 ? 500 : x1 < 3291 ? x1 : 3290;
        x2 = x2 < x1 + 80 ? x1 + 80 : x2 < 3291 ? x2 : 3290;
        y1 = y1 < 16 ? 16 : pal ? (y1 < 311 ? y1 : 310) : (y1 < 257 ? y1 : 256);
        y2 = y2 < y1 + 2 ? y1 + 2 : pal ? (y2 < 313 ? y2 : 312) : (y2 < 259 ? y2 : 258);
        control(0x06000000U | (static_cast<std::uint32_t>(x2) & 0xfffU) << 12U |
                (static_cast<std::uint32_t>(x1) & 0xfffU));
        control(0x07000000U | (static_cast<std::uint32_t>(y2) & 0x3ffU) << 10U |
                (static_cast<std::uint32_t>(y1) & 0x3ffU));
    }
    bool mode_same = previous(0x10) == memory(e + 0x10, 2) && previous(0x12) == memory(e + 0x12, 2);
    for (std::uint32_t i = 0; i < 8; i += 2)
        mode_same = mode_same && previous(i) == memory(e + i, 2);
    if (!mode_same) {
        // GetVideoMode (8004c308) is stored in the environment's +12.
        set_memory(e + 0x12, resident.video_mode, 1);
        std::uint32_t mode = 0x08000000U;
        if ((resident.video_mode & 0xffU) == 1)
            mode |= 8U;
        if (memory(e + 0x11, 1) != 0)
            mode |= 0x10U;
        if (memory(e + 0x10, 1) != 0)
            mode |= 0x20U;
        if (gpu.interlace != 0)
            mode |= 0x80U;
        const auto width = s16(memory(e + 4, 2));
        if (width >= 0x119)
            mode |= width < 0x161 ? 1U : width < 0x191 ? 0x40U : width < 0x231 ? 2U : 3U;
        const auto height = s16(memory(e + 6, 2));
        if (height >= (memory(e + 0x12, 1) != 0 ? 0x121 : 0x101))
            mode |= 0x24U;
        control(mode);
    }
    for (std::uint32_t i = 0; i < gpu.display_environment.size(); ++i)
        gpu.display_environment[i] = static_cast<std::uint8_t>(memory(e + i, 1));
}

// DrawSync(0) (800445d0) -> _sync (80046db4): wait for an empty queue and an
// idle GPU; the waits poll the alarm.
void Program::draw_sync(FrameServices &services) {
    auto &gpu = resident.gpu;
    if (gpu.debug >= 2)
        gpu_print(0x800445f0);
    gpu_alarm(&services);
    // 80046dd4: run queued requests until the queue empties; each pass
    // polls the alarm (80046f30), whose count the recorded result covers.
    // Interrupts that arrived before the next pass (a DMA completion runs
    // the queue too) come first.
    while (gpu.head != gpu.tail) {
        deliver_due_arrivals();
        if (gpu.head != gpu.tail)
            static_cast<void>(gpu_execute());
    }
    gpu.polls = take_service(services.alarm_polls, "DrawSync alarm polls");
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
    const auto bytes = [&](std::string name, std::uint32_t address, auto member) {
        const auto size = (GpuState{}.*member).size();
        for (std::uint32_t i = 0; i < size; ++i)
            add(name, address + i, 1,
                [member, i](Program &p) -> auto & { return (p.resident.gpu.*member)[i]; });
    };
    add("gpu_services", 0x800568c8, 4,
        [](Program &p) -> auto & { return p.resident.gpu.services; });
    for (std::uint32_t i = 0; i < 12; ++i)
        add("gpu_functions", 0x80056888 + 4 * i, 4,
            [i](Program &p) -> auto & { return p.resident.gpu.functions[i]; });
    add("gpu_queued", 0x800568d1, 1, [](Program &p) -> auto & { return p.resident.gpu.queued; });
    add("gpu_debug", 0x800568d2, 1, [](Program &p) -> auto & { return p.resident.gpu.debug; });
    add("gpu_interlace", 0x800568d3, 1,
        [](Program &p) -> auto & { return p.resident.gpu.interlace; });
    add("gpu_width", 0x800568d4, 2, [](Program &p) -> auto & { return p.resident.gpu.width; });
    add("gpu_height", 0x800568d6, 2, [](Program &p) -> auto & { return p.resident.gpu.height; });
    add("gpu_sync_pending", 0x800568d8, 4,
        [](Program &p) -> auto & { return p.resident.gpu.sync_pending; });
    add("gpu_sync_callback", 0x800568dc, 4,
        [](Program &p) -> auto & { return p.resident.gpu.sync_callback; });
    bytes("gpu_draw_environment", 0x800568e0, &GpuState::draw_environment);
    bytes("gpu_display_environment", 0x8005693c, &GpuState::display_environment);
    for (std::uint32_t i = 0; i < 5; ++i)
        add("gpu_registers", 0x800569a0 + 4 * i, 4,
            [i](Program &p) -> auto & { return p.resident.gpu.registers[i]; });
    for (std::uint32_t i = 0; i < 4; ++i)
        add("gpu_otc_registers", 0x800569b4 + 4 * i, 4,
            [i](Program &p) -> auto & { return p.resident.gpu.otc_registers[i]; });
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
    bytes("gpu_packet", 0x8005a238, &GpuState::packet);
    bytes("gpu_control", 0x8005a27c, &GpuState::control);
    // The request queue, one entry per byte.
    for (std::uint32_t i = 0; i < 64 * entry_bytes; ++i)
        add("gpu_queue", queue_base + i, 1,
            [i](Program &p) -> auto & { return p.resident.gpu.queue[i]; });
    for (std::uint32_t i = 0; i < 12; ++i)
        add("disc_image", 0x80059f24 + 4 * i, 4,
            [i](Program &p) -> auto & { return p.resident.disc_read.image[i]; });
}

} // namespace xem::reconstruction
