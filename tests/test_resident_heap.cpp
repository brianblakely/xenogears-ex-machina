// An invented heap exercises allocation modes, headers, held bytes, release and
// coalescing. It describes no original content or observation.
#include "xem/reconstruction/resident_heap.hpp"

#include <iostream>

namespace resident = xem::reconstruction::resident;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <typename Call> void rejects(Call call, const char *message) {
    bool rejected = false;
    try {
        call();
    } catch (const resident::HeapError &) {
        rejected = true;
    }
    check(rejected, message);
}

// Free A (f8 bytes at 80100008), allocated B (f8 bytes, tag 10), free C (f8
// bytes at 80100208), end sentinel D. Held bytes count upward from zero.
resident::Heap sample() {
    resident::Heap heap;
    heap.head = 0x80100008;
    heap.tag = 10;
    heap.headers = {{0x80100000, {0x80100108, 0x84000000}},
                    {0x80100100, {0x80100208, 10U << 21U}},
                    {0x80100200, {0x80100308, 0x84000000}},
                    {0x80100300, {0, resident::heap_end_tag}}};
    std::vector<std::uint8_t> a(0xf8), c(0xf8);
    for (std::size_t i = 0; i < a.size(); ++i) {
        a[i] = static_cast<std::uint8_t>(i);
        c[i] = static_cast<std::uint8_t>(0x80 + i);
    }
    heap.held = {{0x80100008, a}, {0x80100208, c}};
    return heap;
}
constexpr std::uint32_t call_site = 0x80076b00;
constexpr std::uint32_t owner = (call_site & 0x1ffffffU) >> 2U;

void allocation_modes() {
    auto heap = sample();
    heap.allocation_class = 0x25;
    const auto first = resident::heap_allocate(heap, 0x0e, 0, call_site);
    check(first && first->address == 0x80100008 && first->bytes.size() == 0x10 &&
              first->bytes[0xf] == 0xf,
          "First fit rounds up and hands over the block's current bytes");
    check(heap.headers.at(0x80100000) ==
              std::array<std::uint32_t, 2>{0x80100020, 0x25U << 26U | 10U << 21U | owner},
          "The allocated header takes class, tag and caller");
    check(heap.headers.at(0x80100018) == std::array<std::uint32_t, 2>{0x80100108, 0x84000000},
          "The remainder header copies the free header");
    check(heap.allocation_class == 0x20 && heap.last_size == 0x0e && heap.last_caller == call_site,
          "Allocation resets the class and records the request");
    check(heap.held.begin()->first == 0x80100020 && heap.held.begin()->second[0] == 0x18,
          "The remainder header consumed eight held bytes");

    auto exact = sample();
    const auto whole = resident::heap_allocate(exact, 0xf4, 0, call_site);
    check(whole && whole->address == 0x80100008 && exact.held.at(0x801000fc).size() == 4,
          "A four-byte spare allocates the whole block and keeps the slack held");

    auto top = sample();
    const auto high = resident::heap_allocate(top, 0x20, 1, call_site);
    check(high && high->address == 0x80100308 - 0x28 &&
              top.headers.at(0x80100200)[0] == high->address,
          "Mode 1 carves the last fitting block from the top");
    check(top.headers.at(high->address - 8)[0] == 0x80100308, "The carved header links onward");

    // B freed (f8 bytes) and C split into two 78-byte free blocks.
    auto best = sample();
    best.headers.at(0x80100100)[1] = 0x84000000;
    best.headers.at(0x80100200)[0] = 0x80100288;
    best.headers[0x80100280] = {0x80100308, 0x84000000};
    best.held = {{0x80100008, std::vector<std::uint8_t>(0xf8)},
                 {0x80100108, std::vector<std::uint8_t>(0xf8)},
                 {0x80100208, std::vector<std::uint8_t>(0x78)},
                 {0x80100288, std::vector<std::uint8_t>(0x78)}};
    const auto smallest = resident::heap_allocate(best, 0x20, 2, call_site);
    check(smallest && smallest->address == 0x80100208, "Best fit takes the smallest fitting block");
}

void release_and_failure() {
    auto heap = sample();
    auto block = *resident::heap_allocate(heap, 0x10, 0, call_site);
    check(resident::heap_release(heap, block, call_site) == 0 && heap.dirty == 1 &&
              heap.headers.at(0x80100000)[1] == 0x84000000 && block.address == 0,
          "Release frees the header and takes the bytes back");
    resident::HeapBlock twice{0x80100008, {}};
    check(resident::heap_release(heap, twice, call_site) == 0 &&
              heap.headers.at(0x80100000)[1] == 0x84000000,
          "Releasing a free block rewrites its header and keeps its held bytes");

    // The next allocation coalesces the freed block with its free remainder.
    const auto again = resident::heap_allocate(heap, 0xf8, 0, call_site);
    check(again && again->address == 0x80100008 && heap.dirty == 0 &&
              heap.headers.count(0x80100018) == 0 && again->bytes[0x10] == 0x08 &&
              again->bytes[0x11] == 0x01,
          "Coalescing restores the absorbed header bytes into the block");

    auto kept = sample();
    auto held = *resident::heap_allocate(kept, 0x10, 0, call_site);
    kept.headers.at(held.address - 8)[1] |= resident::heap_keep;
    check(resident::heap_release(kept, held, call_site) == -1 && held.bytes.size() == 0x10,
          "A kept block is not released");

    auto full = sample();
    rejects([&] { static_cast<void>(resident::heap_allocate(full, 0x1000, 0, call_site)); },
            "Exhaustion reaches the unreconstructed fatal handler");
    full.quiet = 1;
    check(!resident::heap_allocate(full, 0x1000, 0, call_site), "Quiet exhaustion returns null");
    resident::HeapBlock null{};
    check(resident::heap_release(full, null, call_site) == 1, "A quiet null release returns one");
    full.quiet = 0;
    rejects([&] { static_cast<void>(resident::heap_release(full, null, call_site)); },
            "A non-quiet null release reaches the fatal handler");
    check(full.last_size == 0 && full.last_caller == call_site,
          "The request is recorded before the fatal handler");
    // A stale pointer into free block A: its preceding word is held bytes.
    auto stale = sample();
    stale.held.at(0x80100008)[0x3f] = 0x84;
    resident::HeapBlock inside{0x80100048, {}};
    check(resident::heap_release(stale, inside, call_site) == 0 && stale.dirty == 1 &&
              stale.held.at(0x80100008)[0x3c] == 0 && stale.held.at(0x80100008)[0x3d] == 0 &&
              stale.held.at(0x80100008)[0x3e] == 0 && stale.held.at(0x80100008)[0x3f] == 0x84 &&
              stale.headers.size() == 4,
          "A stale release rewrites the held word before it");
    stale.held.at(0x80100008)[0x3f] = 0x86;
    inside.address = 0x80100048;
    check(resident::heap_release(stale, inside, call_site) == -1, "A stale kept word is honoured");
    resident::HeapBlock unaligned{0x80100049, {}};
    rejects([&] { static_cast<void>(resident::heap_release(stale, unaligned, call_site)); },
            "An unaligned stale release is rejected");
    resident::HeapBlock outside{0x80100110, {}};
    rejects([&] { static_cast<void>(resident::heap_release(stale, outside, call_site)); },
            "A stale word outside the heap's bytes is rejected");
    auto broken = sample();
    broken.headers.erase(0x80100100);
    rejects([&] { static_cast<void>(resident::heap_allocate(broken, 0x1000, 0, call_site)); },
            "A list that leaves the owned headers is rejected");
}
} // namespace

int main() {
    try {
        allocation_modes();
        release_and_failure();
        std::cout << "Resident heap: two source-boundary groups passed\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
