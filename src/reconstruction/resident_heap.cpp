#include "xem/reconstruction/resident_heap.hpp"

#include <iterator>

namespace xem::reconstruction::resident {
namespace {
std::array<std::uint32_t, 2> &header(Heap &heap, std::uint32_t address) {
    const auto found = heap.headers.find(address);
    if (found == heap.headers.end())
        throw HeapError("Heap list reaches an unowned block header");
    return found->second;
}
// A word inside heap-held bytes, or null.
std::uint8_t *held_word(Heap &heap, std::uint32_t address) {
    auto found = heap.held.upper_bound(address);
    if (found == heap.held.begin())
        return nullptr;
    --found;
    if (address + std::uint64_t{4} > found->first + found->second.size())
        return nullptr;
    return found->second.data() + (address - found->first);
}
std::uint32_t tag_of(Heap &heap, std::uint32_t address) {
    return header(heap, address)[1] & heap_tag_mask;
}

// Held byte ranges stay disjoint and merged with their neighbours.
void give(Heap &heap, std::uint32_t address, std::vector<std::uint8_t> bytes) {
    if (bytes.empty())
        return;
    auto next = heap.held.lower_bound(address);
    if (next != heap.held.end() && next->first < address + bytes.size())
        throw HeapError("Heap bytes are already held");
    if (next != heap.held.begin()) {
        auto previous = std::prev(next);
        const auto end = previous->first + previous->second.size();
        if (end > address)
            throw HeapError("Heap bytes are already held");
        if (end == address) {
            address = previous->first;
            auto merged = std::move(previous->second);
            merged.insert(merged.end(), bytes.begin(), bytes.end());
            bytes = std::move(merged);
            heap.held.erase(previous);
        }
    }
    next = heap.held.lower_bound(address);
    if (next != heap.held.end() && next->first == address + bytes.size()) {
        bytes.insert(bytes.end(), next->second.begin(), next->second.end());
        heap.held.erase(next);
    }
    heap.held.emplace(address, std::move(bytes));
}
std::vector<std::uint8_t> take(Heap &heap, std::uint32_t address, std::uint32_t size) {
    auto found = heap.held.upper_bound(address);
    if (found == heap.held.begin())
        throw HeapError("Heap block bytes are not held by the heap");
    --found;
    const auto begin = found->first;
    const auto end = begin + found->second.size();
    if (address + std::uint64_t{size} > end)
        throw HeapError("Heap block bytes are not held by the heap");
    auto range = std::move(found->second);
    heap.held.erase(found);
    const auto offset = address - begin;
    std::vector<std::uint8_t> result(range.begin() + offset, range.begin() + offset + size);
    if (offset != 0)
        heap.held.emplace(begin, std::vector<std::uint8_t>(range.begin(), range.begin() + offset));
    if (offset + size != range.size())
        heap.held.emplace(address + size,
                          std::vector<std::uint8_t>(range.begin() + offset + size, range.end()));
    return result;
}
void new_header(Heap &heap, std::uint32_t address, std::array<std::uint32_t, 2> value) {
    static_cast<void>(take(heap, address, 8)); // The header overwrites held bytes.
    heap.headers[address] = value;
}
[[noreturn]] void fatal(const char *reason) { throw HeapError(reason); }
} // namespace

std::optional<HeapBlock> heap_allocate(Heap &heap, std::uint32_t size, std::uint32_t mode,
                                       std::uint32_t call_site) {
    heap.last_caller = call_site;
    const auto caller = (call_site & 0x1ffffffU) >> 2U;
    if (heap.dirty != 0)
        heap_coalesce(heap);
    heap.last_size = size;
    const auto need = (size + 3U) & ~3U;
    // The allocated header: class (then reset), tag and caller; keep cleared.
    const auto owned_flags = [&] {
        const auto flags = static_cast<std::uint32_t>(heap.allocation_class) << 26U |
                           (heap.tag & 0xfU) << 21U | (caller & 0x1fffffU);
        heap.allocation_class = 0x20;
        return flags;
    };
    const auto whole = [&](std::uint32_t at) {
        header(heap, at)[1] = owned_flags();
        return HeapBlock{at + 8, take(heap, at + 8, need)};
    };
    const auto split_front = [&](std::uint32_t at) {
        auto &current = header(heap, at);
        const auto rest = at + 8 + need;
        const auto copy = current;
        current[0] = rest + 8;
        current[1] = owned_flags();
        new_header(heap, rest, copy);
        return HeapBlock{at + 8, take(heap, at + 8, need)};
    };
    bool none = true;
    std::uint32_t best = 0x800000, candidate = 0, exact = 0;
    auto at = heap.head - 8;
    for (;;) {
        if (tag_of(heap, at) != 0) {
            bool end = false;
            while (!end) {
                if (tag_of(heap, at) == heap_end_tag)
                    end = true;
                else if (at = header(heap, at)[0] - 8; tag_of(heap, at) == 0)
                    break;
            }
            if (end)
                break;
        }
        const auto available = header(heap, at)[0] - at - 0x10;
        const auto spare = static_cast<std::int32_t>(available - need);
        if (spare == 4 || spare == 0) {
            if (mode != 1)
                return whole(at);
            none = false;
            exact = at;
        } else if (spare >= 5) {
            none = false;
            if (mode == 1) {
                candidate = at;
            } else if (mode == 2) {
                if (available < best) {
                    candidate = at;
                    best = available;
                }
            } else {
                return split_front(at);
            }
        }
        at = header(heap, at)[0] - 8;
    }
    if (none) {
        if (heap.quiet != 0)
            return std::nullopt;
        fatal("Heap exhaustion reaches the resident fatal handler 80019acc");
    }
    if (mode != 1)
        return split_front(candidate);
    if (candidate < exact)
        return whole(exact);
    // Mode 1: carve the block from the top of the last fitting free block.
    auto &below = header(heap, candidate);
    const auto next = below[0];
    const auto data = next - (need + 8);
    below[0] = data;
    new_header(heap, data - 8, {next, owned_flags()});
    return HeapBlock{data, take(heap, data, need)};
}

std::int32_t heap_release(Heap &heap, HeapBlock &block, std::uint32_t call_site) {
    if (block.address == 0) {
        if (heap.quiet != 0)
            return 1;
        heap.last_size = 0;
        heap.last_caller = call_site;
        fatal("Releasing a null block reaches the resident fatal handler 80019acc");
    }
    // The original rewrites the word before the block without consulting the
    // list, so a stale pointer into a free block rewrites heap-held bytes.
    if (!heap.headers.contains(block.address - 8)) {
        if ((block.address & 3U) != 0)
            throw HeapError("An unaligned release faults in the original");
        auto *word = held_word(heap, block.address - 4);
        if (word == nullptr)
            throw HeapError("Released block's flags word is not owned by the heap");
        std::uint32_t stale = 0;
        for (std::size_t i = 0; i < 4; ++i)
            stale |= static_cast<std::uint32_t>(word[i]) << (8U * i);
        if ((stale & heap_keep) != 0)
            return -1;
        if (!block.bytes.empty())
            throw HeapError("A stale block's bytes are already held by the heap");
        for (std::size_t i = 0; i < 4; ++i)
            word[i] = static_cast<std::uint8_t>(0x84000000U >> (8U * i));
        heap.dirty = 1;
        block = {};
        return 0;
    }
    auto &flags = header(heap, block.address - 8)[1];
    if ((flags & heap_keep) != 0)
        return -1;
    const bool free = (flags & heap_tag_mask) == 0;
    flags = 0x84000000U;
    heap.dirty = 1;
    // Releasing a free block rewrites its header; its bytes are already held.
    if (free) {
        if (!block.bytes.empty())
            throw HeapError("A free block's bytes cannot also be owned by the caller");
    } else {
        give(heap, block.address, std::move(block.bytes));
    }
    block = {};
    return 0;
}

bool heap_trim(Heap &heap, HeapBlock &block, std::uint32_t size) {
    auto &words = header(heap, block.address - 8);
    if (size + 0x10U >= words[0] - (block.address - 8) - 0x10U)
        return false;
    const auto end = block.address + size;
    if (size > block.bytes.size())
        throw HeapError("A trimmed block's bytes are not the caller's");
    give(heap, end, std::vector<std::uint8_t>(block.bytes.begin() + size, block.bytes.end()));
    block.bytes.resize(size);
    new_header(heap, end, {words[0], 0x84000000U});
    words[0] = end + 8;
    heap.dirty = 1;
    return true;
}

void heap_restart(Heap &heap, std::uint32_t address, ByteRuns &outside) {
    const auto start = address & ~3U;
    const auto first = heap.head - 8;
    const auto first_words = header(heap, first);
    const auto next = first_words[0];
    if (start + 8 > next - 8)
        throw HeapError("A heap restart lies past its first block");
    for (auto at = heap.headers.upper_bound(std::min(start, first));
         at != heap.headers.end() && at->first < next - 8; ++at)
        if (at->first != first)
            throw HeapError("A heap restart crosses another block header");
    const auto header_bytes = [](const std::array<std::uint32_t, 2> &words) {
        std::vector<std::uint8_t> bytes(8);
        for (std::size_t i = 0; i < 8; ++i)
            bytes[i] = static_cast<std::uint8_t>(words[i / 4] >> (8U * (i % 4)));
        return bytes;
    };
    // Move [from, to) out of a byte store, whole or failing.
    const auto take_from = [](ByteRuns &store, std::uint32_t from, std::uint32_t to) {
        std::vector<std::uint8_t> result;
        while (from < to) {
            auto found = store.upper_bound(from);
            if (found == store.begin())
                throw HeapError("A heap restart reaches RAM no Program state holds");
            --found;
            const auto begin = found->first;
            auto bytes = std::move(found->second);
            store.erase(found);
            if (from >= begin + bytes.size())
                throw HeapError("A heap restart reaches RAM no Program state holds");
            const auto end = std::min<std::uint64_t>(to, begin + bytes.size());
            result.insert(result.end(), bytes.begin() + (from - begin),
                          bytes.begin() + static_cast<std::ptrdiff_t>(end - begin));
            if (from != begin)
                store.emplace(begin, std::vector<std::uint8_t>(bytes.begin(),
                                                               bytes.begin() + (from - begin)));
            if (end < begin + bytes.size())
                store.emplace(
                    static_cast<std::uint32_t>(end),
                    std::vector<std::uint8_t>(
                        bytes.begin() + static_cast<std::ptrdiff_t>(end - begin), bytes.end()));
            from = static_cast<std::uint32_t>(end);
        }
        return result;
    };
    const auto word_at = [](const std::vector<std::uint8_t> &bytes) {
        std::uint32_t value = 0;
        for (std::size_t i = 0; i < 4; ++i)
            value |= static_cast<std::uint32_t>(bytes[4 + i]) << (8U * i);
        return value;
    };
    std::uint32_t old_flags = 0;
    heap.headers.erase(first);
    if (start >= first) {
        // The old first header and the held bytes below the new one leave the
        // heap; the new header overwrites held bytes.
        std::vector<std::uint8_t> below;
        if (start == first) {
            old_flags = first_words[1];
        } else {
            if (start < first + 8)
                throw HeapError("A heap restart overlaps the old first header");
            below = header_bytes(first_words);
            if (start > first + 8) {
                const auto held = take(heap, first + 8, start - first - 8);
                below.insert(below.end(), held.begin(), held.end());
            }
            old_flags = word_at(take(heap, start, 8));
        }
        if (!below.empty()) {
            auto &slot = outside[first];
            if (!slot.empty())
                throw HeapError("RAM below a heap restart is already held outside it");
            slot = std::move(below);
        }
    } else {
        // RAM below the old start joins the first block; the old header's
        // words become its bytes.
        auto joined = take_from(outside, start, first);
        old_flags = word_at(joined);
        joined.erase(joined.begin(), joined.begin() + 8);
        const auto old_header = header_bytes(first_words);
        joined.insert(joined.end(), old_header.begin(), old_header.end());
        give(heap, start + 8, std::move(joined));
    }
    heap.headers[start] = {next, (old_flags & 0x021fffffU) | 0x84000000U};
    heap.head = start + 8;
}

void heap_coalesce(Heap &heap) {
    auto at = heap.head - 8;
    while (tag_of(heap, at) != heap_end_tag) {
        if (tag_of(heap, at) == 0) {
            auto &current = header(heap, at);
            while (tag_of(heap, current[0] - 8) == 0) {
                const auto absorbed = current[0] - 8;
                const auto old = header(heap, absorbed);
                current[0] = old[0];
                heap.headers.erase(absorbed);
                std::vector<std::uint8_t> bytes(8);
                for (std::size_t i = 0; i < 8; ++i)
                    bytes[i] = static_cast<std::uint8_t>(old[i / 4] >> (8U * (i % 4)));
                give(heap, absorbed, std::move(bytes));
            }
        }
        at = header(heap, at)[0] - 8;
    }
    heap.dirty = 0;
}

void heap_select_tag(Heap &heap, std::uint32_t tag, std::uint32_t word) {
    if (tag >= heap.tag_words.size())
        throw HeapError("Heap owner tag outside its word table");
    heap.tag = static_cast<std::uint16_t>(tag);
    heap.tag_words[tag] = word;
    heap.quiet = 0;
}

void heap_set_keep(Heap &heap, std::uint32_t block, bool keep) {
    const auto header = heap.headers.find(block - 8);
    if (header == heap.headers.end())
        throw HeapError("The keep flag names a block without a heap header");
    auto &flags = header->second[1];
    flags = keep ? flags | heap_keep : flags & ~heap_keep;
}

} // namespace xem::reconstruction::resident
