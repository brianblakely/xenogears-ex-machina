// The resident mode dispatcher 80019acc(0) of executable dc0b2dd7...: the
// graphics reset, the heap restart, the next mode's BSS and overlay load, the
// second heap restart and the call of the mode's function.
#include "xem/reconstruction/battle.hpp"
#include "xem/reconstruction/packed_field.hpp"
#include "xem/reconstruction/program.hpp"

#include <array>

namespace xem::reconstruction {
namespace {
// Mode table 8001808c (16 bytes a row): the mode's function, its BSS start
// and end, and whether its overlay is loaded.
struct ModeRow {
    std::uint32_t function, bss_start, bss_end, loaded;
};
constexpr std::array<ModeRow, 7> mode_rows{{
    {0x8001a4b4, 0x800592b8, 0x8006faec, 0},
    {0x80077e88, 0x800af5e4, 0x800c426c, 1},
    {0x8001b6c4, 0x800c3a6c, 0x800d39f0, 1},
    {0x80070cfc, 0x8009bbb0, 0x8009d80c, 1},
    {0x80088e90, 0x800925d0, 0x8009b554, 1},
    {0x8001c634, 0x800592b8, 0x8006faec, 0},
    {0x800737ec, 0x80076f38, 0x80077454, 1},
}};
// 8004eaa0: each mode's overlay file in directory 1.
constexpr std::array<std::uint32_t, 7> mode_files{0, 0xe, 0x10, 0xf, 0xd, 0x11, 0x12};
// 80018084: where an overlay is decoded.
constexpr std::uint32_t overlay_destination = 0x8006faf0;

void observed(const ProgramObserver &observe, const Program &program, std::string_view operation,
              std::uint32_t address) {
    if (observe)
        observe(program, {operation, address, {}, {}}, true);
}
const ModeRow &row_of(std::uint32_t mode) {
    if (mode >= mode_rows.size())
        throw MissingDependency({"mode_dispatch", 0x80019afc, {}, {}}, "symbol:mode-table-row",
                                false, "The mode table has no such row");
    return mode_rows[mode];
}
// Remove [from, to) from a byte store, where it holds any.
void discard(resident::ByteRuns &store, std::uint32_t from, std::uint32_t to) {
    for (auto at = store.begin(); at != store.end();) {
        const auto begin = at->first;
        const auto end = begin + static_cast<std::uint32_t>(at->second.size());
        if (end <= from || begin >= to) {
            ++at;
            continue;
        }
        auto bytes = std::move(at->second);
        at = store.erase(at);
        if (begin < from)
            store.emplace(begin,
                          std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + (from - begin)));
        if (end > to)
            store.emplace(to, std::vector<std::uint8_t>(bytes.begin() + (to - begin), bytes.end()));
    }
}
} // namespace

// 8003223c: release each block from the head up to the end block (kept
// blocks stay); the mode block keeps its pointer.
void Program::release_heap_blocks() {
    auto &heap = resident.heap;
    for (auto at = heap.head - 8;
         (heap.headers.at(at)[1] & resident::heap_tag_mask) != resident::heap_end_tag;) {
        const auto next = heap.headers.at(at)[0];
        auto &mode = resident.mode_block;
        if (at + 8 == mode.address && !mode.bytes.empty()) {
            resident::HeapBlock block{mode.address, std::move(mode.bytes)};
            if (resident::heap_release(heap, block, 0x8003227c) == -1)
                mode.bytes = std::move(block.bytes);
            else
                mode.bytes.clear();
        } else {
            static_cast<void>(release_owned_block(at + 8, 0x8003227c));
        }
        at = next - 8;
    }
}

// 80031b10(address): release every block (8003223c), coalesce when dirty,
// restart the list at `address` and clear tag 10's word.
void Program::restart_heap(std::uint32_t address) {
    release_heap_blocks();
    if (resident.heap.dirty != 0)
        resident::heap_coalesce(resident.heap);
    resident::heap_restart(resident.heap, address, resident.heap_outside);
    resident.heap.tag_words[10] = 0;
}

// 800199cc(mode): unless cached, allocate the mode's overlay file (tag 6,
// from the top, quietly) in directory 1 and start reading it; the heap tag
// and quiet flag and the selected directory are restored.
std::uint32_t Program::load_mode_block(std::uint32_t mode) {
    if (resident.mode_loaded != mode) {
        resident.mode_loaded = mode;
        auto &heap = resident.heap;
        const auto tag = heap.tag;
        // 800284b4: the selected directory's position in the directory table.
        auto &read = resident.disc_read;
        std::uint32_t base = 0, index = 0;
        for (std::uint32_t entry = 0; entry < 0x40; ++entry) {
            if (entry * 2 + 2 > read.directories.size())
                throw field::FieldFormatError("The directory table is not loaded");
            if (static_cast<std::uint32_t>(read.directories[entry * 2] |
                                           read.directories[entry * 2 + 1] << 8U) ==
                read.directory + 1) {
                base = entry & ~3U;
                index = entry - base;
                break;
            }
        }
        heap.tag = 6;
        static_cast<void>(select_directory(0, 1));
        const auto quiet = heap.quiet;
        heap.quiet = 1;
        if (mode >= mode_files.size())
            throw MissingDependency({"mode_block", 0x80019a3c, {}, {}}, "symbol:mode-file-table",
                                    false, "The mode file table has no such entry");
        const auto file = mode_files[mode];
        auto block = resident::heap_allocate(heap, file_size(static_cast<std::int32_t>(file)), 1,
                                             0x80019a4c);
        if (!block) {
            resident.mode_block = {};
            resident.mode_loaded = 0xffffffffU;
        } else {
            resident.mode_block = std::move(*block);
            static_cast<void>(
                read_file(static_cast<std::int32_t>(file), resident.mode_block.address, 0, 0));
        }
        heap.quiet = quiet;
        static_cast<void>(select_directory(base, index));
        heap.tag = tag;
    }
    return resident.mode_block.address;
}

std::uint32_t Program::mode_dispatch(FrameServices &services, DispatchStep from,
                                     const ProgramObserver &observe) {
    const auto &row = row_of(resident.next_mode);
    auto &gpu = resident.gpu;
    switch (from) {
    case DispatchStep::start:
        reset_graph(1);
        if (gpu.debug >= 2) // 800444d8 DrawSyncCallback(0)
            throw MissingDependency({"gpu_debug_print", 0x80044508, {}, {}},
                                    "symbol:printf-80019964", false,
                                    "libgpu request checking prints are not reconstructed");
        gpu.sync_callback = 0;
        resident.pad.hook = 0; // 800363f0(0)
        draw_sync(services);
        vertical_sync(services); // VSync(2): the same stores once two blanks passed
        observed(observe, *this, "mode_heap", 0x80019b3c);
        [[fallthrough]];
    case DispatchStep::heap:
        restart_heap(row.bss_end + 0x800);
        // 80019c7c: 80032498(10, 0).
        resident::heap_select_tag(resident.heap, 10, 0); // 80032498
        if (row.loaded == 0)
            throw MissingDependency({"mode_dispatch", 0x80019bd0, {}, {}}, "symbol:mode-resident",
                                    false, "Modes without an overlay are not reconstructed");
        if (row.function != 0x8001b6c4)
            throw MissingDependency({"mode_dispatch", 0x80019b64, {}, {}},
                                    "symbol:mode-overlay-state", false,
                                    "Only the battle mode's overlay memory is reconstructed");
        {
            // 80019560: the words after the BSS start up to and including the
            // BSS end.
            const auto from_address = row.bss_start + 4;
            const auto to_address = row.bss_end + 4;
            discard(resident.heap_outside, from_address, to_address);
            auto &memory = battle ? *battle : battle.emplace();
            for (const auto &[at, bytes] : memory.regions)
                if (at < to_address && from_address < at + bytes.size())
                    throw field::FieldFormatError("A mode's BSS overlaps battle memory");
            memory.regions.emplace(from_address,
                                   std::vector<std::uint8_t>(to_address - from_address));
        }
        static_cast<void>(load_mode_block(resident.next_mode));
        observed(observe, *this, "mode_loaded", 0x80019b7c);
        [[fallthrough]];
    case DispatchStep::wait:
        disc_wait(0); // 80028a60(0)
        observed(observe, *this, "mode_decode", 0x80019b90);
        {
            // 80032eb4(block, 8006faf0).
            const auto &block = resident.mode_block;
            if (block.address == 0 || block.bytes.empty())
                throw field::FieldFormatError("The mode block holds no overlay");
            // The decoder reads its final flag past a block that ends with its
            // data: the next heap header.
            auto source = block.bytes;
            const auto after = resident.heap.headers.find(
                block.address + static_cast<std::uint32_t>(block.bytes.size()));
            if (after != resident.heap.headers.end())
                for (std::uint32_t i = 0; i < 8; ++i)
                    source.push_back(
                        static_cast<std::uint8_t>(after->second[i / 4] >> (8U * (i % 4))));
            auto decoded = field::decode_packed_block(source);
            auto &memory = battle ? *battle : battle.emplace();
            const auto end = overlay_destination + decoded.data.size();
            for (const auto &[at, bytes] : memory.regions)
                if (at < end && overlay_destination < at + bytes.size())
                    throw field::FieldFormatError("A decoded overlay overlaps battle memory");
            memory.regions.emplace(overlay_destination, std::move(decoded.data));
        }
        observed(observe, *this, "mode_decoded", 0x80019b98);
        [[fallthrough]];
    case DispatchStep::sync:
        draw_sync(services);
        vertical_sync(services);
        // 800404d4 EnterCriticalSection: a BIOS system call.
        draw_sync(services);
        vertical_sync(services);
        // 80040454 FlushCache and 800404e4 ExitCriticalSection: BIOS calls.
        // The interrupts the critical section held off are taken as it ends.
        deliver_leading_arrivals();
        // 80019548 moves the stack to the end of RAM.
        observed(observe, *this, "mode_reinit", 0x80019bdc);
        [[fallthrough]];
    case DispatchStep::reinit:
        restart_heap(row.bss_end + 4);
        // 80031a30.
        resident.w_593a0 = 0; // 8003747c(0)
        resident.heap.allocation_class = 0x20;
        resident.heap.tag = 10;
        resident.w_59334 = 0;
        resident.w_59338 = 0;
        resident.input_queue.reset(); // 80035db0
        set_next_mode(0);             // 8001996c(0)
        observed(observe, *this, "mode_row", 0x80019c04);
        return row.function;
    }
    throw field::FieldFormatError("Unknown mode dispatch step");
}

} // namespace xem::reconstruction
