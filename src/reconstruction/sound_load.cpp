// Resident sound driver calls of the music load (executable dc0b2dd7...): the
// wave bank load 800380d0 (800381f4, 800393b8, 80039784, 80038264,
// 80039024, 800392ec, 80039248) and its chunked upload 8003827c, and the
// sequence calls 80039850 (8003bb40, 80038f18, 8003b0ac, 8003b22c, 8003b370,
// 8003b930, 8003b424, 8003b9e4), 80039a80 and 8003a89c with the reverb
// setting 80038934. Driver objects are addressed by their original
// addresses (sound_memory.hpp); the staged wave bank header is a heap block
// the field owns. BIOS DisableEvent/EnableEvent around list updates keep no
// Program state. The driver's error handler 8003f6b0 and paths the frozen
// route does not reach stop with MissingDependency.
#include "sound_memory.hpp"

#include <algorithm>
#include <functional>

namespace xem::reconstruction {
namespace {
using Memory = detail::SoundMemory;

std::int32_t s32(std::uint32_t v) { return static_cast<std::int32_t>(v); }
std::int32_t s16(std::uint32_t v) { return static_cast<std::int16_t>(v & 0xffffU); }
std::int32_t s8(std::uint32_t v) { return static_cast<std::int8_t>(v & 0xffU); }

[[noreturn]] void missing(const char *operation, std::uint32_t address) {
    throw MissingDependency({operation, address, {}, {}}, "symbol:" + detail::sound_hex(address),
                            false, "A sound driver path is not reconstructed");
}
// 8003f6b0: the driver's error handler (stops the music, plays an error
// sequence); reaching it ends the reconstructed run.
[[noreturn]] void driver_error(std::uint32_t code, std::uint32_t site) {
    throw MissingDependency({"sound_driver_error", site, {}, {}},
                            "symbol:sound-error-" + detail::sound_hex(code), false,
                            "The sound driver error handler 8003f6b0 is not reconstructed");
}

// 80039024 (top: the highest fitting gap, signed sizes) and 80038f18 (the
// first fitting gap, unsigned sizes): allocate `size` bytes in the sound pool
// after a 16-byte header {2, 0, end, next}, linked after the block before the
// gap, and zero them (800392ec); zero when no gap fits. Freed blocks' stale
// objects and headers that the new block covers stop being owned.
std::uint32_t pool_allocate(Memory &m, std::uint32_t size, bool top) {
    auto &d = m.d;
    const auto header = [&](std::uint32_t at) -> std::array<std::uint32_t, 4> & {
        const auto found = d.pool_headers.find(at);
        if (found == d.pool_headers.end())
            throw resident::SoundError("Sound pool list leaves its owned headers");
        return found->second;
    };
    const auto need = ((size + 15U) & ~15U) + 16U;
    const auto limit = m.u32(0x800595e4);
    std::uint32_t before = 0;
    std::uint32_t place = 0;
    if (top) {
        std::uint32_t ceiling = 0;
        for (auto at = d.pool;;) {
            const auto next = header(at)[3];
            if (next == 0) {
                if (s32(limit - header(at)[2]) >= s32(need)) {
                    before = at;
                    ceiling = limit;
                }
                break;
            }
            if (s32(next - header(at)[2]) >= s32(need)) {
                before = at;
                ceiling = next;
            }
            at = next;
        }
        if (before == 0)
            return 0;
        place = (ceiling - need + 15U) & ~15U;
    } else {
        auto at = d.pool;
        while (header(at)[3] != 0 && header(at)[3] - header(at)[2] < need)
            at = header(at)[3];
        if (header(at)[3] == 0 && limit - header(at)[2] < need)
            return 0;
        before = at;
        place = (header(at)[2] + 15U) & ~15U;
    }
    const auto object = place + 16U;
    const auto end = std::uint64_t{object} + size;
    std::erase_if(d.pool_headers, [&](const auto &entry) {
        return entry.first < end && place < entry.first + 16U;
    });
    std::erase_if(d.objects, [&](const auto &entry) {
        return entry.first < end && place < entry.first + entry.second.size();
    });
    const auto next = header(before)[3];
    d.pool_headers[place] = {2, 0, object + size, next};
    header(before)[3] = place;
    d.objects[object].assign(size, 0);
    return object;
}

// The SPU memory allocation list at 8006f9fc: 12 entries {in use 80, 0,
// next entry (s16), SPU address, size}; entry 0 heads it.
class SpuBlocks {
  public:
    explicit SpuBlocks(resident::SoundDriver &driver) : table(driver.spu_blocks) {}
    std::uint32_t field(std::uint32_t entry, std::uint32_t offset, std::uint32_t width) const {
        if (entry >= table.size() / 16)
            throw resident::SoundError("SPU allocation list leaves its table");
        std::uint32_t value = 0;
        for (std::uint32_t i = 0; i < width; ++i)
            value |= static_cast<std::uint32_t>(table[entry * 16 + offset + i]) << (8U * i);
        return value;
    }
    std::uint32_t next(std::uint32_t entry) const {
        return static_cast<std::uint32_t>(s16(field(entry, 2, 2)));
    }
    std::uint32_t address(std::uint32_t entry) const { return field(entry, 4, 4); }
    std::uint32_t end(std::uint32_t entry) const { return address(entry) + field(entry, 8, 4); }
    // 80039784 picks the first unused entry, else entry 0; the new entry
    // follows `previous`.
    std::uint32_t add(std::uint32_t previous, std::uint32_t at, std::uint32_t size) {
        std::uint32_t entry = 0;
        for (std::uint32_t i = 0; i < 12; ++i)
            if (field(i, 0, 1) == 0) {
                entry = i;
                break;
            }
        store(entry, 0, 0x80, 1);
        store(entry, 1, 0, 1);
        store(entry, 4, at, 4);
        store(entry, 8, size, 4);
        store(entry, 2, field(previous, 2, 2), 2);
        store(previous, 2, entry, 2);
        return at;
    }

  private:
    std::array<std::uint8_t, 0xc0> &table;
    void store(std::uint32_t entry, std::uint32_t offset, std::uint32_t value,
               std::uint32_t width) {
        for (std::uint32_t i = 0; i < width; ++i)
            table[entry * 16 + offset + i] = static_cast<std::uint8_t>(value >> (8U * i));
    }
};

// 800393b8: allocate `size` bytes of SPU memory in the first gap after an
// allocation; the address, or 0.
std::uint32_t spu_allocate(resident::SoundDriver &d, std::uint32_t size) {
    SpuBlocks blocks(d);
    auto end = blocks.end(0);
    std::uint32_t previous = 0;
    for (std::uint32_t visited = 0; blocks.next(previous) != 0; ++visited) {
        if (visited == 12)
            throw resident::SoundError("SPU allocation list does not terminate");
        const auto entry = blocks.next(previous);
        if (s32(blocks.address(entry) - end) >= s32(size))
            return blocks.add(previous, end, size);
        previous = entry;
        end = blocks.end(entry);
    }
    if (s32(0x80000U - end) < s32(size))
        return 0;
    return blocks.add(previous, end, size);
}

// 800395b8: allocate `size` bytes of SPU memory at `at`; `at`, or 0 when
// that range is not free.
std::uint32_t spu_allocate_at(resident::SoundDriver &d, std::uint32_t size, std::uint32_t at) {
    SpuBlocks blocks(d);
    const auto stop = at + size;
    auto end = blocks.end(0);
    std::uint32_t previous = 0;
    std::uint32_t gap = 0;
    if (s32(blocks.address(0)) < s32(at))
        for (std::uint32_t visited = 0;; ++visited) {
            if (visited == 12)
                throw resident::SoundError("SPU allocation list does not terminate");
            const auto entry = blocks.next(previous);
            if (entry == 0) {
                gap = 0x80000U - end;
                break;
            }
            if (s32(blocks.address(entry)) >= s32(stop)) {
                gap = blocks.address(entry) - end;
                break;
            }
            end = blocks.end(entry);
            const bool below = s32(blocks.address(entry)) < s32(at);
            previous = entry;
            if (!below)
                break;
        }
    if (s32(gap) < s32(size) || s32(at) < s32(end))
        return 0;
    return blocks.add(previous, at, size);
}

// 8003b370: a sequence's playback state, after freeing its child blocks
// (8003b930).
void reset_sequence(Memory &m, std::uint32_t seq) {
    if (auto child = m.u32(seq + 4); child != 0) {
        m.w32(seq + 4, 0);
        while (child != 0) {
            const auto next = m.u32(child + 4);
            resident::free_pool_block(m.d, child);
            child = next;
        }
    }
    for (const auto [offset, value] : {std::pair{0x32U, 1U},
                                       {0x36U, 1U},
                                       {0x3aU, 0x30U},
                                       {0x30U, 0U},
                                       {0x34U, 0U},
                                       {0x38U, 4U},
                                       {0x3cU, 4U},
                                       {0x3eU, 4U},
                                       {0x6cU, 0U},
                                       {0x78U, 0U},
                                       {0x84U, 0U},
                                       {0x90U, 0U},
                                       {0x60U, 0U}})
        m.w16(seq + offset, value);
    for (const auto [offset, value] : {std::pair{0x64U, 0x1000000U},
                                       {0x70U, 0x7f000000U},
                                       {0x58U, 0x660000U},
                                       {0x54U, 0x6600U},
                                       {0x28U, 0U},
                                       {0x24U, 0U},
                                       {0x20U, 0U},
                                       {0x48U, 0U},
                                       {0x7cU, 0U},
                                       {0x88U, 0U},
                                       {0x5cU, 0U},
                                       {0x50U, 0x10000U}})
        m.w32(seq + offset, value);
    m.w8(seq + 0x1a, 0);
    m.w8(seq + 0x1b, 0);
}
// 8003b22c: copy the sequence header's attributes; with reverb enabled
// (flag 1000) apply its reverb type, depth, delay and feedback (80038934),
// then reset the playback state (8003b370).
void sequence_attributes(Memory &m, std::uint32_t seq,
                         const std::function<void(std::uint16_t, std::uint16_t)> &depth) {
    const auto data = m.u32(seq + 8);
    m.w16(seq + 0x10, m.u16(seq + 0x10) | 1U);
    m.w16(seq + 0x12, m.u16(data + 0x10));
    m.w8(seq + 0x14, m.u8(data + 0x14));
    m.w16(seq + 0x16, m.u16(data + 0x16));
    m.w16(seq + 0x18, m.u16(data + 0x18));
    m.w8(seq + 0x41, m.u8(data + 0x1a));
    m.w16(seq + 0x44, m.u8(data + 0x1b) << 8U);
    m.w8(seq + 0x42, m.u8(data + 0x1c));
    m.w8(seq + 0x43, m.u8(data + 0x1d));
    if ((m.d.flags & 0x1000U) != 0) {
        // 80038934(type, depth, delay, feedback).
        auto type = static_cast<std::uint32_t>(s8(m.u8(seq + 0x41)));
        auto level = static_cast<std::uint32_t>(s16(m.u16(seq + 0x44)));
        auto delay = m.u8(seq + 0x42);
        auto feedback = m.u8(seq + 0x43);
        if (type != 0xfffffffeU) {
            if (type == 0)
                level = delay = feedback = 0;
            else if (type == 0xffffffffU)
                type = m.u8(0x80059409);
            // 8004e774: the reverb type the SPU library set; another type
            // (or none) reallocates the reverb work area.
            if (m.u32(0x800589b8) != type || type == 0)
                missing("reverb_type_change", 0x800389bc);
            m.w8(0x80059409, type);
            m.d.reverb = static_cast<std::uint16_t>(level); // 8005a3ec
            m.w8(0x8005940a, delay);
            m.w8(0x8005940b, feedback);
            resident::reapply_volumes(m.d); // 80038df4
            depth(m.d.reverb_pair[0], m.d.reverb_pair[1]);
            // 8004e5a0 and 8004e6b8: delay and feedback apply only in the
            // reverb modes 7 and 8.
            if (const auto mode = s32(m.u32(0x800589b8)); mode == 7 || mode == 8)
                missing("spu_reverb_delay", 0x8004e5a0);
        }
    }
    reset_sequence(m, seq);
}

// 8003b424: initialize the voice record of each track with event data
// (halfword offsets from data + 22) and claim hardware voice index - 1.
void sequence_voices(Memory &m, std::uint32_t seq) {
    auto &d = m.d;
    auto count = m.u8(seq + 0x14);
    if (count == 0)
        return;
    const auto data = m.u32(seq + 8);
    auto bank = resident::find_wave_bank(d, static_cast<std::uint32_t>(s16(m.u16(seq + 0x16))));
    if (bank == 0)
        bank = d.wave_banks;
    std::uint32_t mask = 0;
    auto offsets = data + 0x22;
    auto record = seq + resident::voice_records;
    std::uint32_t index = 0;
    auto channel = 0xffffffffU;
    for (; count != 0;
         --count, offsets += 2, record += resident::voice_stride, ++index, ++channel) {
        const auto start = m.u16(offsets);
        if (start == 0) {
            m.w16(record, 0);
            continue;
        }
        const auto bit = 1U << (index & 31U);
        mask |= bit;
        m.w16(record, (bit & m.u32(seq + 0x4c)) != 0 ? 0x421U : 0x401U);
        if ((m.u16(seq + 0x10) & 4U) != 0)
            m.w16(record, m.u16(record) | 4U);
        m.w16(record + 2, 0x170);
        m.w16(record + 4, 0);
        m.w8(record + 7, 0x10);
        m.w8(record + 6, index);
        m.w32(record + 8, m.u16(data + 0x10));
        m.w16(record + 0x66, 0x3c);
        m.w16(record + 0x62, 0xf);
        m.w16(record + 0x72, 0xffff);
        m.w16(record + 0x76, 0x6000);
        m.w32(record + 0x78, 0x7f000000);
        m.w32(record + 0x18, 0);
        m.w32(record + 0x1c, 0);
        m.w16(record + 0x20, 0);
        m.w8(record + 0x22, 0);
        m.w16(record + 0x5c, 0);
        m.w8(record + 0x60, 0);
        m.w16(record + 0x6e, 0);
        m.w8(record + 0x64, 0);
        m.w16(record + 0x74, 0x4000);
        m.w16(record + 0x70, 0);
        for (const auto offset : {0xd0U, 0xd2U, 0xd4U, 0x3cU, 0x3eU, 0xceU})
            m.w16(record + offset, 0);
        m.w32(record + 0x10, data + start);
        m.w32(record + 0x14, data + start);
        for (const auto offset : {0x156U, 0x136U, 0x116U, 0xf6U})
            m.w16(record + offset, 0);
        m.w32(record + 0x2c, bank);
        m.w8(record + 0x25, m.u8(seq + 0x16));
        if (bank != 0)
            resident::load_instrument(d, 0, record); // 8003e5bc
        m.w8(record + 0x27, channel);
        m.w16(record + 0x32, 0);
        m.w16(record + 0x34, 0x100);
        resident::claim_voice(d, record + 0x30, channel); // 8003e724
    }
    m.w32(seq + 0x48, mask);
}

} // namespace

// 8003827c: queue the next part (at most `bytes`) of the wave bank upload
// (80059584 SPU address, 80059588 bytes left) from `data`; the bytes left.
std::uint32_t Program::continue_wave_upload(std::uint32_t data, std::uint32_t bytes) {
    Memory m(resident.sound);
    const auto left = m.u32(0x80059588);
    if (left == 0)
        return 0;
    const auto size = s32(bytes) < s32(left) ? bytes : left;
    const auto spu = m.u32(0x80059584);
    spu_transfer(spu, data, size, 0); // 8003bc10
    m.w32(0x80059584, spu + size);
    m.w32(0x80059588, left - size);
    return left - size;
}

std::uint32_t Program::load_wave_bank(std::uint32_t header, std::uint32_t bytes,
                                      std::uint32_t mode) {
    auto &driver = resident.sound;
    if (resident::find_wave_bank(driver, memory(header + 0x20, 2)) != 0) // 800383ec
        driver_error(0x16, 0x80038160);
    // 800381f4: mode 0 takes the header's fixed address (+28), -1 none.
    if (mode == 0)
        mode = memory(header + 0x28);
    else if (mode == 0xffffffffU)
        mode = 0;
    const auto spu = mode == 0 ? spu_allocate(driver, memory(header + 0x14))
                               : spu_allocate_at(driver, memory(header + 0x14), mode);
    if (spu == 0)
        driver_error(0x1f, 0x80038160);
    Memory m(driver);
    m.w32(0x80059584, spu); // 80038264
    m.w32(0x80059588, memory(header + 0x14));
    const auto header_bytes = memory(header + 0x10);
    static_cast<void>(continue_wave_upload(header + memory(header + 0x18), bytes - header_bytes));
    const auto bank = pool_allocate(m, header_bytes, true); // 80039024
    if (bank == 0)
        driver_error(0x1e, 0x80038160);
    for (std::uint32_t i = 0; i < header_bytes; ++i) // 80039248
        m.w8(bank + i, memory(header + i, 1));
    m.w32(bank + 0x28, spu);
    // Link it last on the wave bank list.
    if (driver.wave_banks == 0) {
        driver.wave_banks = bank;
    } else {
        auto at = driver.wave_banks;
        while (m.u32(at + 0x2c) != 0)
            at = m.u32(at + 0x2c);
        m.w32(at + 0x2c, bank);
    }
    m.w32(bank + 0x2c, 0);
    return bank;
}

std::uint32_t Program::open_sequence(std::uint32_t data) {
    Memory m(resident.sound, &resident.disc_transfers);
    const auto tracks = m.u8(data + 0x14);
    const auto table = m.u8(data + 0x15);
    auto size = resident::voice_records + tracks * resident::voice_stride; // 8003bb40
    if (table != 0)
        size += 0x180;
    const auto seq = pool_allocate(m, size, false); // 80038f18
    if (seq == 0)
        driver_error(0x1e, 0x800398ac);
    m.w32(seq + 8, data);
    if (table != 0) {
        // 8003b0ac: five-byte entries from data + (data + 20): an index,
        // then the word stored at that index of the table after the voices.
        const auto base = seq + resident::voice_records + tracks * resident::voice_stride;
        m.w32(seq + 0xc, base);
        auto entry = data + m.u16(data + 0x20);
        for (auto left = table; left != 0; --left, entry += 5)
            m.w32(base + m.u8(entry) * 4U, m.u8(entry + 1) | m.u8(entry + 2) << 8U |
                                               m.u8(entry + 3) << 16U | m.u8(entry + 4) << 24U);
    }
    const auto depth = [&](std::uint16_t left, std::uint16_t right) {
        spu_reverb_depth(left, right);
    };
    sequence_attributes(m, seq, depth);
    // Arrivals recorded since the poll's call or this call's entry precede
    // the voice setup; those during it follow it.
    deliver_arrivals(0x80085c90);
    deliver_arrivals(0x80039850);
    sequence_voices(m, seq);
    deliver_arrivals(0x8003b424);
    m.w32(seq + 0x4c, 0);
    // 8003b9e4: link it first on the sequence list.
    m.w32(seq, resident.sound.sequences);
    resident.sound.sequences = seq;
    return seq;
}

void Program::start_sequence(std::uint32_t sequence, std::uint32_t volume, std::uint32_t frames) {
    if (sequence == 0)
        driver_error(5, 0x80039aa4);
    Memory m(resident.sound, &resident.disc_transfers);
    // The playing flag is cleared first, so the stop (80039c4c) that
    // follows a set flag is never reached.
    m.w16(sequence + 0x10, m.u16(sequence + 0x10) & 0x7fffU);
    const auto depth = [&](std::uint16_t left, std::uint16_t right) {
        spu_reverb_depth(left, right);
    };
    sequence_attributes(m, sequence, depth);
    deliver_arrivals(0x8003b424);
    deliver_arrivals(0x80039a80);
    sequence_voices(m, sequence);
    deliver_arrivals(0x8003b424);
    m.w32(sequence + 0x70, 0);
    sequence_volume(sequence, volume, frames);
    m.w16(sequence + 0x10, m.u16(sequence + 0x10) | 0x8000U);
}

void Program::sequence_volume(std::uint32_t sequence, std::uint32_t volume, std::uint32_t frames) {
    Memory m(resident.sound);
    m.w16(sequence + 0x7a, volume << 8U);
    if (frames == 0) {
        m.w32(sequence + 0x70, volume << 24U);
        m.w16(sequence + 0x78, 0);
        resident::mark_sequence_voices(resident.sound, sequence, 0x100); // 8003e680
    } else {
        const auto change =
            (volume << 16U) - static_cast<std::uint32_t>(s32(m.u32(sequence + 0x70)) >> 8);
        if (change == 0)
            return;
        m.w16(sequence + 0x78, frames);
        m.w32(sequence + 0x74, static_cast<std::uint32_t>(s32(change) / s32(frames)) << 8U);
    }
    if ((m.u16(sequence + 0x10) & 0x100U) != 0 && volume != 0)
        missing("sequence_resume", 0x8003aa30);
}

} // namespace xem::reconstruction
