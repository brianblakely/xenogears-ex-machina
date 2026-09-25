#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>

namespace xem::reconstruction::resident {

class HeapError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// Resident heap of executable dc0b2dd7...: 80031bdc allocate, 800320e8 release,
// 80031ff8 coalesce, 800320b8 clear keep. Blocks form a list of 8-byte headers
// placed before each block: the next block's data address, then flags with the
// caller's word address (bits 0-20), owner tag (21-24; 0 free, 1 end), keep
// (25) and class (26-31).
//
// The heap owns its headers and the bytes it holds: free blocks and the slack
// of exact-fit blocks. Allocation hands the block's current bytes to the caller
// (the original does not initialize them); release hands them back.
struct Heap {
    std::uint32_t head{};                 // 80059320: first block's data address
    std::uint16_t allocation_class{0x20}; // 80059318; each allocation resets it to 20
    std::uint16_t tag{};                  // 8005931c: owner tag of new blocks
    std::uint32_t dirty{};                // 8005932c: coalesce before allocating
    std::uint32_t quiet{};                // 80059330: failures return zero
    std::uint32_t last_size{};            // 8005933c
    std::uint32_t last_caller{};          // 80059340: caller return address - 8
    // 80059fa4: a word per owner tag (0-12; resident 80034eac's text record
    // follows at 80059fd8); 80032498(tag, word) selects the tag, stores its
    // word and clears `quiet`.
    std::array<std::uint32_t, 13> tag_words{};
    std::map<std::uint32_t, std::array<std::uint32_t, 2>> headers; // address -> next, flags
    std::map<std::uint32_t, std::vector<std::uint8_t>> held;       // heap-owned byte ranges
};

inline constexpr std::uint32_t heap_tag_mask = 0x1e00000;
inline constexpr std::uint32_t heap_end_tag = 0x200000;
inline constexpr std::uint32_t heap_keep = 0x2000000;

struct HeapBlock {
    std::uint32_t address{};
    std::vector<std::uint8_t> bytes;
};

// Allocate `size` bytes (rounded up to a word) in mode 0 (first fit), 1 (last
// fit from the top, preferring an exact block below it) or 2 (best fit).
// `call_site` is the original JAL address that reaches 80031bdc (ra - 8).
// nullopt only in quiet mode; otherwise failure reaches the resident fatal
// handler 80019acc, which is not reconstructed.
[[nodiscard]] std::optional<HeapBlock> heap_allocate(Heap &heap, std::uint32_t size,
                                                     std::uint32_t mode, std::uint32_t call_site);
// 800320e8. Returns 0 and moves the block's bytes into the heap; -1 for a kept
// block, which stays with the caller; 1 for a null block in quiet mode.
// `call_site` (ra - 8) is recorded before a non-quiet null release fails.
[[nodiscard]] std::int32_t heap_release(Heap &heap, HeapBlock &block, std::uint32_t call_site);
// 80031ff8: merge runs of free blocks; absorbed headers become held bytes.
void heap_coalesce(Heap &heap);
// Bytes of RAM by address, outside every other owner.
using ByteRuns = std::map<std::uint32_t, std::vector<std::uint8_t>>;
// 80031b10 after its release pass and coalesce: the list restarts with a
// free class-21 header at `address & ~3` whose next is the first block's
// next and whose caller and keep bits are those of the word already there
// (+4). RAM between the old and the new first header moves between the heap
// and `outside`: the heap gives up what lies below a higher start and takes
// in what lies below the old start.
void heap_restart(Heap &heap, std::uint32_t address, ByteRuns &outside);

} // namespace xem::reconstruction::resident
