// Resident sound driver tick of executable dc0b2dd7...: 8003c028, which the
// BIOS root-counter event path calls four times per frame, and its call tree:
// master-volume slides (8003c484, 80038e6c), hardware voice register updates
// (8003e900), the sequencer (8003c4c4 per-tick slides, 8003c6e8 event reader
// with the opcode table 80050624), modulation (8003efe4 with the shapes at
// 800508a4), voice mixing (8003ebf0, 8003eea0) and key on/off (8003ef04,
// 8003efa0, 8003eb5c), with GetRCnt 80040690 timing and SpuSetIRQ 8004d600.
//
// Driver objects are addressed by their original addresses, as the original
// does. SPU register stores become HardwareWrite records; SPU and root counter
// loads are platform inputs consumed in program order. Paths not reached on
// the frozen route stop with MissingDependency naming their address.
#include "sound_memory.hpp"

#include <limits>
#include <string>
#include <type_traits>

namespace xem::reconstruction {
namespace {
using resident::SoundDriver;
using resident::SoundError;

std::string hex(std::uint32_t value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string text(8, '0');
    for (int i = 7; i >= 0; --i, value >>= 4)
        text[static_cast<std::size_t>(i)] = digits[value & 15U];
    return text;
}
[[noreturn]] void missing(const char *operation, std::uint32_t address) {
    throw MissingDependency({operation, address, {}, {}}, "symbol:" + hex(address), false,
                            "A sound driver path is not reconstructed");
}
std::int32_t s8(std::uint32_t v) { return static_cast<std::int8_t>(v & 0xffU); }
std::int32_t s16(std::uint32_t v) { return static_cast<std::int16_t>(v & 0xffffU); }
std::int32_t s32(std::uint32_t v) { return static_cast<std::int32_t>(v); }
std::uint32_t u(std::int32_t v) { return static_cast<std::uint32_t>(v); }
// R3000 arithmetic: products keep their low word, shifts of signed values
// are arithmetic, and a division by zero yields -1 or 1.
std::uint32_t mul(std::uint32_t a, std::uint32_t b) {
    return static_cast<std::uint32_t>(std::int64_t{s32(a)} * std::int64_t{s32(b)});
}
std::uint32_t sra(std::uint32_t v, std::uint32_t n) { return u(s32(v) >> (n & 31U)); }
std::uint32_t quotient(std::uint32_t a, std::uint32_t b) {
    const auto x = s32(a);
    const auto y = s32(b);
    if (y == 0)
        return x >= 0 ? 0xffffffffU : 1U;
    if (x == std::numeric_limits<std::int32_t>::min() && y == -1)
        return a;
    return u(x / y);
}

using Memory = detail::SoundMemory;

constexpr std::uint32_t voice_stride = 0x158;
constexpr std::uint32_t tick_count = 0x80059540;
constexpr std::uint32_t tick_time = 0x800595c4;
constexpr std::uint32_t requests = 0x8005955c;
constexpr std::uint32_t noise = 0x800594e4;
constexpr std::uint32_t common = 0x8005a3c0; // SpuCommonAttr, then two slides
constexpr std::uint32_t spu_base = 0x800508e4;

class Tick {
  public:
    Tick(ResidentState &state) : resident(state), m(state.sound, &state.disc_transfers) {}

    // 8003c028; `event` is V0 at entry, the driver flags the event handler
    // loaded.
    std::uint32_t run(std::uint32_t event) {
        if ((event & 0x40U) != 0)
            return 0;
        const auto start = counter(0xf2000002);
        const auto stamp = m.d.start_stamp;
        m.d.start_stamp = stamp + 1;
        if ((stamp & 1U) != 0)
            master_slides();
        voice_registers();
        for (auto seq = m.d.sequences; seq != 0; seq = m.u32(seq))
            if (s16(m.u16(seq + 0x10)) < 0)
                sequence_time(seq);
        for (auto seq = m.d.sequences; seq != 0; seq = m.u32(seq)) {
            if (s16(m.u16(seq + 0x10)) >= 0)
                continue;
            const auto count = m.u8(seq + 0x14);
            if (count == 0)
                continue;
            modulate(seq + 0x94, count);
            resident::update_voices(m.d, seq, seq + 0x94, count); // 8003ebf0
        }
        keys();
        if (const auto flags = m.u16(requests); (flags & 1U) != 0) {
            m.w16(requests, flags & 0xfffeU);
            spu_irq_enable();
        }
        const auto end = counter(0xf2000002);
        if (end < start)
            return 0;
        m.w32(tick_time, end - start + m.u32(tick_time));
        m.w32(tick_count, m.u32(tick_count) + 1);
        return 0;
    }

    // 8004cb3c, the SPU DMA completion callback: leave transfer mode, then
    // run the library's completion callback, the driver's queue step
    // 8003bb64.
    void transfer_completed() {
        // 8004d208 (a busy delay when 80058e58 is zero) keeps no state.
        const auto control = m.d.spu_registers + 0x1aa;
        spu_write(control, spu_read(0x8004cb64, 2) & 0xffcfU);
        if ((spu_read(0x8004cb74, 2) & 0x30U) != 0)
            for (std::uint32_t tries = 1; tries < 0xf01; ++tries)
                if ((spu_read(0x8004cb98, 2) & 0x30U) == 0)
                    break;
        const auto callback = m.u32(0x80058e40);
        if (callback == 0)
            missing("spu_transfer_event", 0x80040e18);
        if (callback != 0x8003bb64)
            missing("spu_transfer_callback", callback);
        // 8003bb64: the finished queue entry's own callback, then the next
        // queued transfer.
        const auto index = m.u16(0x80059510);
        const auto entry = m.u32(0x80059458) + index * 20;
        m.d.flags |= 4U;
        switch (const auto done = m.u32(entry + 16); done) {
        case 0:
            break;
        case 0x80038b4c:
            upload_continue();
            break;
        default:
            missing("spu_transfer_entry_callback", done);
        }
        m.d.flags &= 0xffefU;
        if (m.u16(0x80059510) != m.u16(0x800594f4))
            next_transfer();
        m.d.flags &= 0xfffbU;
    }

    // 80038b4c: queue the next chunk (at most 800h bytes, the whole rest when
    // under 841h) of a chunked SPU upload; after the last, free the staging
    // block and restore the reverb settings.
    void upload_continue() {
        const auto left = m.u32(0x800595e0);
        if (left == 0) {
            free_pool_block(m.d, m.u32(0x800595a4));
            m.w32(0x800595a4, 0);
            reverb_depth(m.d.reverb_pair[0], m.d.reverb_pair[1]);
            // 8004e5a0 and 8004e6b8: delay and feedback apply only in the
            // reverb modes 7 and 8.
            if (const auto mode = s32(m.u32(0x800589b8)); mode == 7 || mode == 8)
                missing("spu_reverb_delay", 0x8004e5a0);
            m.d.flags &= 0xffdfU;
            return;
        }
        const auto size = s32(left) < 0x841 ? left : 0x800U;
        const auto spu = m.u32(0x800595dc);
        const auto block = m.u32(0x800595a4);
        m.w32(0x800595e0, left - size);
        m.w32(0x800595dc, spu + size);
        enqueue(1, spu, block, size, 0x80038b4c);
        if ((m.d.flags & 0x10U) == 0)
            enqueue(1, spu, block, size, 0);
    }

    // 8003bca0: append a transfer to the queue and start it when idle.
    // Outside the transfer callback (flag 4 clear) the original first waits
    // for a free entry (Program::spu_transfer) and holds a BIOS critical
    // section around this, which keeps no Program state.
    void enqueue(std::uint32_t type, std::uint32_t spu, std::uint32_t ram, std::uint32_t size,
                 std::uint32_t callback) {
        auto end = (m.u16(0x800594f4) + 1U) & 0xffffU;
        if (end >= 8)
            end = 0;
        m.w16(0x800594f4, end);
        const auto entry = m.u32(0x80059458) + end * 20;
        m.w16(entry, type & 0xfU);
        m.w16(entry + 2, 0);
        m.w32(entry + 4, ram);
        m.w32(entry + 8, spu & 0x7fff8U);
        m.w32(entry + 12, size);
        m.w32(entry + 16, callback);
        if ((m.d.flags & 0x10U) == 0)
            next_transfer();
    }

    // 8004e574: the reverb depth, left and right.
    void reverb_depth(std::uint16_t left, std::uint16_t right) {
        const auto base = m.d.spu_registers;
        spu_write(base + 0x184, left);
        spu_write(base + 0x186, right);
        m.d.spu_reverb_output = {left, right};
    }

    // 8003bdbc: nonzero while six or more transfers are queued.
    bool queue_full() {
        auto in = m.u16(0x800594f4);
        const auto out = m.u16(0x80059510);
        if (in < out)
            in += 8;
        return s32((in & 0xffffU) - out) >= 6;
    }

    // 8003be68: start the next queued transfer (SPU DMA write).
    void next_transfer() {
        auto index = (m.u16(0x80059510) + 1U) & 0xffffU;
        if (index >= 8)
            index = 0;
        m.w16(0x80059510, index);
        m.d.flags |= 0x10U;
        const auto entry = m.u32(0x80059458) + index * 20;
        // 8004d964: the completion callback.
        if (m.u32(0x80058e40) != 0x8003bb64)
            missing("spu_transfer_callback_change", 0x8004d964);
        // 8004d930(0): DMA transfer mode.
        m.w32(0x800589a4, 0);
        m.w32(0x80058e24, 0);
        // 8004d8d8: the SPU transfer address in 8-byte units.
        const auto address = m.u32(entry + 8);
        if (address - 0x1010U <= 0x7efe8U) {
            auto aligned = address;
            if (m.u32(0x80058e2c) != 0) {
                const auto unit = m.u32(0x80058e34);
                if (unit == 0)
                    missing("spu_alignment_break", 0x8004d09c);
                if (aligned % unit != 0)
                    aligned = (aligned + unit) & ~m.u32(0x80058e38);
            }
            m.w16(0x80058e20, (aligned >> (m.u32(0x80058e30) & 31U)) & 0xffffU);
        }
        if (m.u16(entry) != 1)
            missing("spu_transfer_type", 0x8003bf14);
        // 8004d878 -> 8004cf38: DMA write of up to 7eff0h bytes.
        auto size = m.u32(entry + 12);
        if (size > 0x7eff0U)
            size = 0x7eff0U;
        if (m.u32(0x80058e24) != 0)
            missing("spu_transfer_io", 0x8004c970);
        const auto base = m.d.spu_registers;
        const auto shift = m.u32(0x80058e30) & 31U;
        // 8004cca8(2): transfer address register.
        const auto start = ((m.u16(0x80058e20) << shift) >> shift) & 0xffffU;
        m.w16(0x80058e20, start);
        spu_write(base + 0x1a6, start);
        // 8004cca8(1): wait until it reads back, then select DMA write.
        m.w32(0x80058e58, 0);
        if (latch(base + 0x1a6, 0x8004cd48) != start)
            missing("spu_transfer_address_timeout", 0x8004cd60);
        spu_write(base + 0x1aa, (latch(base + 0x1aa, 0x8004cd8c) & 0xffcfU) | 0x20U);
        // 8004cca8(3): wait for the mode, set the SPU delay, start DMA 4.
        if ((latch(base + 0x1aa, 0x8004ce38) & 0x30U) != 0x20U)
            missing("spu_transfer_mode_timeout", 0x8004ce4c);
        const auto delay = m.u32(0x80058e1c);
        const auto timing = (latch(delay, 0x8004d1bc) & 0xf0ffffffU) | 0x20000000U;
        resident.hardware_writes.push_back({delay, timing, 4});
        const auto ram = m.u32(entry + 4);
        m.w32(0x80058e5c, ram);
        const auto blocks = (size >> 6U) + ((size & 63U) != 0 ? 1U : 0U);
        m.w32(0x80058e60, blocks);
        resident.hardware_writes.push_back({m.u32(0x80058e0c), ram, 4});
        resident.hardware_writes.push_back({m.u32(0x80058e10), (blocks << 16U) | 0x10U, 4});
        resident.hardware_writes.push_back({m.u32(0x80058e14), 0x01000201U, 4});
    }

    // A register only software changes, read back: the recorded load at
    // `site` when the platform input supplies it next (captures that record
    // it do so wherever it runs), else its last recorded write in this run,
    // else the observed I/O page (SPU registers are not in it).
    std::uint32_t latch(std::uint32_t address, std::uint32_t site) {
        const bool spu = address - 0x1f801c00U < 0x400U;
        if (!resident.platform.empty() &&
            resident.platform.front().kind == PlatformInput::Kind::read &&
            resident.platform.front().site == site)
            return platform_read(resident.platform, site, spu ? 2 : 4);
        for (auto write = resident.hardware_writes.rbegin();
             write != resident.hardware_writes.rend(); ++write)
            if (write->address == address)
                return write->value;
        const auto offset = address - 0x1f801000U;
        if (spu || offset >= 0xc00U || (address & 3U) != 0)
            missing("unrecorded_register_read", site);
        std::uint32_t value = 0;
        for (std::uint32_t i = 0; i < 4; ++i)
            value |= static_cast<std::uint32_t>(resident.io[offset + i]) << (8U * i);
        return value;
    }

  private:
    ResidentState &resident;
    Memory m;

    // Hardware.
    std::uint32_t spu_read(std::uint32_t site, std::uint32_t width) {
        return platform_read(resident.platform, site, width);
    }
    void spu_write(std::uint32_t address, std::uint32_t value) {
        if (address - 0x1f801c00U >= 0x400U || (address & 1U) != 0)
            throw field::FieldFormatError("SPU register outside the observed I/O page");
        resident.hardware_writes.push_back({address, value & 0xffffU, 2});
    }
    std::uint32_t spu() { return m.u32(spu_base); }
    // 80040690 GetRCnt: the counter value of root counters 0-2.
    std::uint32_t counter(std::uint32_t spec) {
        if ((spec & 0xffffU) >= 3)
            return 0;
        return platform_read(resident.platform, 0x800406b0, 2);
    }

    // 8003c484: step a slide {value, step, count, target}.
    void slide(std::uint32_t at) {
        const auto count = (m.u16(at + 8) - 1U) & 0xffffU;
        m.w16(at + 8, count);
        m.w32(at, count != 0 ? m.u32(at) + m.u32(at + 4) : u(s16(m.u16(at + 10))) << 16U);
    }

    // The odd-tick master volume slides into the common attributes.
    void master_slides() {
        if (m.u16(common + 0x38) != 0) {
            slide(common + 0x30);
            const auto volume = u(s16(m.u16(common + 0x32)));
            m.w16(common + 0x28, volume);
            // 80038e6c(volume, common + 4, 0): left and right, one inverted
            // by the phase flags 200/400.
            const auto pair = common + 4;
            m.w16(pair + 2, volume);
            m.w16(pair, volume);
            const auto flags = m.u16(0x8005957c);
            if ((flags & 0x600U) != 0)
                m.w16((flags & 0x200U) != 0 ? pair + 2 : pair, 0U - volume);
            m.w32(common, m.u32(common) | 3U);
        }
        if (m.u16(common + 0x44) != 0) {
            slide(common + 0x3c);
            const auto volume = m.u16(common + 0x3e);
            m.w16(common + 0x2a, volume);
            m.w16(common + 0x12, volume);
            m.w16(common + 0x10, volume);
            m.w32(common, m.u32(common) | 0xc0U);
        }
        if (m.u32(common) != 0) {
            common_attributes();
            m.w32(common, 0);
        }
    }

    // 8004d988 SpuSetCommonAttr(common): master volumes and modes, CD and
    // external input volumes, and the CD/external reverb and mix enables of
    // the control register, each when its mask bit is set (a zero mask sets
    // them all).
    void common_attributes() {
        const auto mask = m.u32(common);
        const bool all = mask == 0;
        const auto base = m.d.spu_registers;
        const auto master = [&](std::uint32_t enable, std::uint32_t mode_bit, std::uint32_t mode_at,
                                std::uint32_t volume_at, std::uint32_t reg) {
            if (!all && (mask & enable) == 0)
                return;
            std::uint32_t mode = 0;
            if (all || (mask & mode_bit) != 0) {
                const auto selected = m.u16(common + mode_at);
                if (selected < 8 && selected != 0)
                    mode = 0x8000U + (selected - 1U) * 0x1000U;
            }
            auto volume = m.u16(common + volume_at);
            if (mode != 0) {
                const auto level = s16(volume);
                volume = level >= 128 ? 127U : level < 0 ? 0U : u(level);
            }
            spu_write(base + reg, (volume & 0x7fffU) | mode);
        };
        master(1, 4, 8, 4, 0x180);
        master(2, 8, 10, 6, 0x182);
        const auto copy = [&](std::uint32_t bit, std::uint32_t at, std::uint32_t reg) {
            if (all || (mask & bit) != 0)
                spu_write(base + reg, m.u16(common + at));
        };
        copy(0x40, 16, 0x1b0);
        copy(0x80, 18, 0x1b2);
        copy(0x400, 28, 0x1b4);
        copy(0x800, 30, 0x1b6);
        const auto control = [&](std::uint32_t bit, std::uint32_t at, std::uint32_t flag,
                                 std::uint32_t clear_site, std::uint32_t set_site) {
            if (!all && (mask & bit) == 0)
                return;
            const bool on = m.u32(common + at) != 0;
            const auto value = spu_read(on ? set_site : clear_site, 2);
            spu_write(base + 0x1aa, on ? value | flag : value & ~flag & 0xffffU);
        };
        control(0x100, 20, 4, 0x8004dbec, 0x8004dc04);
        control(0x200, 24, 1, 0x8004dc40, 0x8004dc58);
        control(0x1000, 32, 8, 0x8004dc94, 0x8004dcac);
        control(0x2000, 36, 2, 0x8004dce8, 0x8004dd00);
    }

    // 8004d600 SpuSetIRQ(1): enable the SPU interrupt and wait until the
    // control register shows it.
    void spu_irq_enable() {
        const auto control = m.d.spu_registers + 0x1aa;
        spu_write(control, spu_read(0x8004d6b4, 2) | 0x40U);
        if ((spu_read(0x8004d6c4, 2) & 0x40U) != 0)
            return;
        for (std::uint32_t tries = 1; tries < 0xf01; ++tries)
            if ((spu_read(0x8004d714, 2) & 0x40U) != 0)
                return;
        missing("spu_irq_timeout", 0x8004d6f8);
    }

    // 8003e900: write each claimed hardware voice's changed registers.
    void voice_registers() {
        const auto base = spu();
        std::uint32_t changed = 0;
        std::uint32_t reverb = 0;
        std::uint32_t noise_on = 0;
        std::uint32_t fm = 0;
        for (std::uint32_t channel = 0; channel < 24; ++channel) {
            const auto owner = m.d.voice_owners[channel];
            if (owner == 0)
                continue;
            const auto regs = base + channel * 16;
            const auto dirty = m.u16(owner + 6);
            if (dirty != 0) {
                if ((dirty & 1U) != 0) {
                    spu_write(regs, m.u16(owner + 8));
                    spu_write(regs + 2, m.u16(owner + 10));
                }
                if ((dirty & 4U) != 0)
                    spu_write(regs + 4, m.u16(owner + 20));
                if ((dirty & 8U) != 0) {
                    spu_write(regs + 6, m.u32(owner + 28) >> 3U);
                    spu_write(regs + 14, m.u32(owner + 32) >> 3U);
                }
                if ((dirty & 0x10U) != 0) {
                    const auto low = spu_read(0x8003e9c0, 1);
                    spu_write(regs + 8,
                              low + (m.u8(owner + 39) << 8U) + ((m.u8(owner + 36) >> 2U) << 15U));
                }
                if ((dirty & 0x20U) != 0) {
                    const auto value = spu_read(0x8003e9e8, 2);
                    spu_write(regs + 8, (value & 0xff0fU) + (m.u8(owner + 40) << 4U));
                }
                if ((dirty & 0x40U) != 0) {
                    const auto value = spu_read(0x8003ea10, 2);
                    spu_write(regs + 10, (value & 0x3fU) + (m.u8(owner + 41) << 6U) +
                                             ((m.u8(owner + 37) >> 1U) << 14U));
                }
                if ((dirty & 0x80U) != 0) {
                    const auto value = spu_read(0x8003ea44, 2);
                    spu_write(regs + 10, (value & 0xffc0U) + m.u8(owner + 42) +
                                             ((m.u8(owner + 38) >> 2U) << 5U));
                }
                if ((dirty & 0x100U) != 0) {
                    const auto value = spu_read(0x8003ea70, 2);
                    spu_write(regs + 8, (value & 0xfff0U) + m.u8(owner + 43));
                }
                changed |= dirty & 0x7000U;
                m.w16(owner + 6, 0);
            }
            const auto modes = m.u16(owner + 2);
            fm |= ((modes >> 4U) & 1U) << channel;
            noise_on |= ((modes >> 5U) & 1U) << channel;
            reverb |= ((modes >> 6U) & 1U) << channel;
        }
        if ((changed & 0xffffU) != 0) {
            if ((changed & 0x1000U) != 0) {
                spu_write(base + 0x190, fm);
                spu_write(base + 0x192, fm >> 16U);
            }
            if ((changed & 0x2000U) != 0) {
                spu_write(base + 0x194, noise_on);
                spu_write(base + 0x196, noise_on >> 16U);
            }
            if ((changed & 0x4000U) != 0) {
                spu_write(base + 0x198, reverb);
                spu_write(base + 0x19a, reverb >> 16U);
            }
        }
        if (const auto on = m.d.voice_holds; on != 0) {
            spu_write(base + 0x188, on);
            spu_write(base + 0x18a, on >> 16U);
            m.d.voice_holds = 0;
        }
    }

    // 8003eb5c: release the ADSR of voices keyed off, then key them off.
    void keys() {
        const auto base = spu();
        if (const auto released = m.d.voice_changes; released != 0)
            for (std::uint32_t channel = 0; channel < 24; ++channel)
                if ((released & (1U << channel)) != 0)
                    spu_write(base + 10 + channel * 16, (spu_read(0x8003eb90, 2) & 0xffc0U) | 6U);
        const auto off = m.d.voice_releases | m.d.voice_changes;
        if (off != 0) {
            spu_write(base + 0x18c, off);
            spu_write(base + 0x18e, off >> 16U);
            m.d.voice_changes = 0;
            m.d.voice_releases = 0;
        }
    }

    // The per-tick part of the sequence loop of 8003c028.
    void sequence_time(std::uint32_t seq) {
        const auto end = m.u32(seq + 0x2c);
        if (end != 0 && m.u32(seq + 0x24) >= end)
            missing("sequence_end", 0x8003ae84);
        if (m.u16(seq + 0x6c) != 0) {
            slide(seq + 0x64);
            m.w32(seq + 0x54, mul(u(s16(m.u16(seq + 0x5a))), u(s16(m.u16(seq + 0x66)))));
        }
        if (m.u16(seq + 0x78) != 0) {
            slide(seq + 0x70);
            flag_voices(seq, 2, 0x100);
        }
        if (m.u16(seq + 0x84) != 0) {
            slide(seq + 0x7c);
            flag_voices(seq, 2, 0x200);
        }
        if (m.u16(seq + 0x90) != 0) {
            slide(seq + 0x88);
            flag_voices(seq, 2, 0x100);
        }
        m.w32(seq + 0x20, m.u32(seq + 0x20) + 1);
        m.w32(seq + 0x28, u(s16(m.u16(seq + 0x66))) + m.u32(seq + 0x28));
        auto budget = m.u32(seq + 0x50) - m.u32(seq + 0x54);
        m.w32(seq + 0x50, budget);
        while (s32(budget) < 0) {
            const auto ticks = (m.u16(seq + 0x36) - 1U) & 0xffffU;
            m.w16(seq + 0x36, ticks);
            m.w32(seq + 0x50, m.u32(seq + 0x50) + 0x10000);
            if (ticks == 0) {
                m.w16(seq + 0x36, m.u16(seq + 0x3a));
                const auto beat = (m.u16(seq + 0x34) + 1U) & 0xffffU;
                m.w16(seq + 0x34, beat);
                if (m.u16(seq + 0x38) < beat) {
                    m.w16(seq + 0x34, 1);
                    m.w16(seq + 0x32, m.u16(seq + 0x32) + 1);
                }
            }
            if (const auto count = m.u8(seq + 0x14); count != 0) {
                slides(seq, seq + 0x94, count);
                events(seq, seq + 0x94, count);
            }
            if (m.u32(seq + 0x48) == 0) {
                m.w16(seq + 0x10, m.u16(seq + 0x10) & 0x7fffU);
                return;
            }
            m.w32(seq + 0x24, m.u32(seq + 0x24) + 1);
            if (m.u32(seq + 0x70) == 0) {
                stop_sequence(m.d, seq);
                m.w16(seq + 0x10, m.u16(seq + 0x10) | 0x100U);
            }
            if (m.u16(seq + 0x32) == m.u16(seq + 0x1e)) {
                m.w16(seq + 0x10, m.u16(seq + 0x10) & 0xffdfU);
                // 8003a838(seq, 0, 0): tempo scale back to 100.
                m.w16(seq + 0x6e, 0x100);
                m.w16(seq + 0x6c, 0);
                m.w32(seq + 0x64, 0x1000000);
                m.w32(seq + 0x54, mul(u(s16(m.u16(seq + 0x5a))), 0x100));
                m.w16(seq + 0x1e, 0);
            }
            budget = m.u32(seq + 0x50);
        }
    }

    // 8003e680 (field +2) and 8003e6c0 (field +36): set bits in each active
    // voice.
    void flag_voices(std::uint32_t seq, std::uint32_t field, std::uint32_t bits) {
        auto voice = seq + 0x94;
        for (auto count = m.u8(seq + 0x14); count != 0; --count, voice += voice_stride)
            if (m.u16(voice) != 0)
                m.w16(voice + field, m.u16(voice + field) | bits);
    }

    // 8003c4c4: per-tick tempo slide, then each voice's pitch, volume and
    // pan slides and its note and gate counters.
    void slides(std::uint32_t seq, std::uint32_t voices, std::uint32_t count) {
        if (auto left = m.u16(seq + 0x60); left != 0) {
            left = (left - 1U) & 0xffffU;
            m.w32(seq + 0x58,
                  left != 0 ? m.u32(seq + 0x58) + m.u32(seq + 0x5c) : m.u16(seq + 0x62) << 16U);
            m.w16(seq + 0x60, left);
            m.w32(seq + 0x54, mul(u(s16(m.u16(seq + 0x5a))), u(s16(m.u16(seq + 0x66)))));
        }
        for (auto voice = voices; count != 0; --count, voice += voice_stride) {
            const auto state = m.u16(voice);
            if (state == 0)
                continue;
            const auto counters = m.u32(voice + 0x5c);
            auto changes = m.u16(voice + 2);
            auto note = counters & 0xffffU;
            auto gate = counters >> 16U;
            if (note == 0) {
                m.w16(voice + 2, changes);
                continue;
            }
            auto active = m.u16(voice + 4);
            if ((active & 8U) != 0) {
                const auto left = (m.u16(voice + 0x96) + 0xffffU) & 0xffffU;
                m.w16(voice + 0x96, left);
                changes |= 0x100U;
                if (left == 0)
                    active &= 0xfff7U;
                m.w32(voice + 0x78, m.u32(voice + 0x78) + m.u32(voice + 0x88));
            }
            if ((active & 1U) != 0) {
                changes |= 0x200U;
                if ((active & 2U) == 0) {
                    const auto left = (m.u16(voice + 0x94) + 0xffffU) & 0xffffU;
                    m.w16(voice + 0x94, left);
                    if (left == 0)
                        active &= 0xfffeU;
                }
                m.w32(voice + 0x68, m.u32(voice + 0x68) + m.u32(voice + 0x84));
            }
            if ((active & 0x10U) != 0) {
                const auto left = (m.u16(voice + 0x98) + 0xffffU) & 0xffffU;
                m.w16(voice + 0x98, left);
                std::uint32_t value = 0;
                if (left == 0) {
                    value = m.u16(voice + 0x92);
                    active &= 0xffefU;
                } else {
                    value = m.u16(voice + 0x74) + m.u16(voice + 0x90);
                }
                m.w16(voice + 0x74, value);
                changes |= 0x100U;
            }
            if ((active & 0x20U) != 0) {
                const auto left = (m.u16(voice + 0x9a) + 0xffffU) & 0xffffU;
                m.w16(voice + 0x9a, left);
                std::uint32_t value = 0;
                if (left == 0) {
                    value = m.u16(voice + 0x8e);
                    active &= 0xffdfU;
                } else {
                    value = m.u16(voice + 0x76) + m.u16(voice + 0x8c);
                }
                m.w16(voice + 0x76, value);
                changes |= 0x100U;
            }
            m.w16(voice + 4, active);
            note -= 1U;
            gate -= 1U;
            if (note == 1 && (state & 0x1000U) != 0) {
                m.w8(voice + 0x5a, 6);
                m.w16(voice + 0x36, m.u16(voice + 0x36) | 0x80U);
            }
            if (gate == 0) {
                m.w16(voice, m.u16(voice) | 0x400U);
                changes |= 2U;
            }
            m.w32(voice + 0x5c, note + (gate << 16U));
            m.w16(voice + 2, changes);
        }
    }

    // 8003cc84: a drum-kit note: instrument, key and pan from the kit table.
    void drum(std::uint32_t seq, std::uint32_t voice, std::uint32_t note) {
        const auto entry = (note & 0xffU) * 4 + m.u32(seq + 0xc);
        load_instrument(m.d, m.u8(entry), voice);
        const auto key =
            (m.u8(entry + 1) << 8U) + u(s16(m.u16(voice + 0x6e))) + u(s16(m.u16(voice + 0x6c)));
        m.w32(voice + 0x68, key << 16U);
        m.w16(voice + 2, m.u16(voice + 2) | 0x100U);
        m.w16(voice + 0x74, m.u8(entry + 3) << 8U);
    }

    // 8003c6e8: read each voice's events until it plays or rests.
    void events(std::uint32_t seq, std::uint32_t voices, std::uint32_t count) {
        for (auto voice = voices; count != 0; --count, voice += voice_stride) {
            if (m.u16(voice) == 0 || m.u16(voice + 0x5c) != 0)
                continue;
            bool played = false;
            const auto saved = m.u16(voice);
            auto at = m.u32(voice + 0x14);
            m.w16(voice, saved & 0xf8ffU);
            for (;;) {
                const auto code = m.u8(at++);
                if (code < 0x80) {
                    if ((m.u16(voice) & 8U) == 0)
                        m.w16(voice + 0x76, code << 8U);
                    m.w16(voice + 2, m.u16(voice + 2) | 0x100U);
                    auto length = m.u8(at++);
                    const auto key = (m.u8(voice + 0x66) + m.u8(0x80050a94 + length)) & 0xffU;
                    m.w8(voice + 0x65, key);
                    const auto fixed = m.u8(0x800509b0 + length);
                    length = fixed != 0 ? fixed : m.u8(at++);
                    m.w16(voice + 0x5c, length);
                    m.w8(voice + 0x5a, m.u8(voice + 0x28));
                    m.w16(voice + 0x36, m.u16(voice + 0x36) | 0x80U);
                    if ((m.u16(voice) & 0x10U) != 0) {
                        drum(seq, voice, key);
                    } else {
                        const auto pitch =
                            (key << 8U) + u(s16(m.u16(voice + 0x6e))) + u(s16(m.u16(voice + 0x6c)));
                        m.w32(voice + 0x68, pitch << 16U);
                    }
                    m.w16(voice + 2, m.u16(voice + 2) | 0x200U);
                    m.w16(voice, m.u16(voice) | 0x180U);
                    played = true;
                    if ((saved & 0x400U) != 0)
                        m.w16(voice + 2, m.u16(voice + 2) | 1U);
                    if (const auto state = m.u16(voice); (state & 0x8000U) != 0) {
                        m.w16(voice, state & 0x7fffU);
                        m.w16(voice + 0x36, 0xffff);
                        m.w16(voice + 2, m.u16(voice + 2) | 0x300U);
                    }
                } else {
                    at = opcode(code, at, seq, voice);
                    if (m.u16(voice) == 0) {
                        m.w32(seq + 0x48, m.u32(seq + 0x48) & ~(1U << (m.u8(voice + 6) & 31U)));
                        break;
                    }
                }
                if ((m.u16(voice) & 0x500U) != 0)
                    break;
            }
            m.w32(voice + 0x14, at);
            if (m.u16(voice) == 0)
                continue;
            lookahead(voice, at);
            gate(voice);
            if (!played)
                continue;
            if ((m.u16(voice + 4) & 4U) != 0) {
                const auto distance = (m.u8(voice + 0x65) - m.u8(voice + 0x64)) << 24U;
                if (distance != 0) {
                    const auto step = quotient(distance, m.u16(voice + 0x70));
                    m.w16(voice + 4, m.u16(voice + 4) | 1U);
                    m.w16(voice + 0x94, m.u16(voice + 0x70));
                    const auto pitch = (m.u8(voice + 0x64) << 8U) + u(s16(m.u16(voice + 0x6e))) +
                                       u(s16(m.u16(voice + 0x6c)));
                    m.w32(voice + 0x68, pitch << 16U);
                    m.w32(voice + 0x84, step);
                }
            }
            m.w8(voice + 0x64, m.u8(voice + 0x65));
            if ((m.u16(voice + 4) & 0x100U) != 0) {
                m.w16(voice + 0x96, m.u16(voice + 0x80));
                m.w32(voice + 0x88, m.u32(voice + 0x7c));
                m.w32(voice + 0x78, m.u16(voice + 0x82) << 16U);
                m.w16(voice + 4, m.u16(voice + 4) | 8U);
            }
            for (std::uint32_t lfo = voice + 0xd8; lfo < voice + 0x158; lfo += 0x20) {
                const auto flags = m.u16(lfo + 0x1e);
                if ((flags & 3U) != 3)
                    continue;
                m.w32(lfo + 4, 0);
                m.w16(lfo + 0x10, 1);
                m.w16(lfo + 0x14, m.u16(lfo + 0x16));
                m.w16(lfo + 0x18, m.u16(lfo + 0x1a));
                m.w16(voice + 2, m.u16(voice + 2) | 0x100U);
                m.w16(lfo + 0x1e, flags & 0xfff3U);
            }
        }
    }

    // 8003c950..8003ca80: after a note, look ahead through the following
    // events to see whether the next one is a note (flag 1000: legato).
    void lookahead(std::uint32_t voice, std::uint32_t at) {
        if ((m.u16(voice) & 0x800U) != 0)
            m.w16(voice, m.u16(voice) | 0x200U);
        auto code = m.u8(at);
        auto frame = voice + m.u16(voice + 0x72) * 12 + 0x9c;
        bool note = code < 0x80;
        while (!note) {
            if (code == 0x90) {
                at = m.u32(voice + 0x18);
                if (at == 0)
                    break;
            } else if (code == 0x80 || code == 0xb0 || code == 0xb1) {
                m.w16(voice, m.u16(voice) & 0xfdffU);
                break;
            } else if (code == 0x81) {
                m.w16(voice, m.u16(voice) | 0x200U);
                break;
            } else if (code == 0x99 && m.u8(frame) != 0) {
                at = m.u32(frame + 4);
            } else {
                if (code == 0x99)
                    frame -= 12;
                if (code == 0x9a && m.u8(frame) == 0) {
                    at = m.u32(frame + 8);
                    frame -= 12;
                } else {
                    at += m.u8(0x80050824 + code - 0x80);
                }
            }
            code = m.u8(at);
            note = code < 0x80;
        }
        m.w16(voice, note ? m.u16(voice) | 0x1000U : m.u16(voice) & 0xefffU);
    }

    // 8003ca84..8003cb28: the gate (key-off point) of the new note.
    void gate(std::uint32_t voice) {
        const auto adjust = m.u8(voice + 0x60);
        auto length = u(s8(adjust)) + m.u16(voice + 0x5c);
        auto result = length;
        if (s16(length) <= 0) {
            result = m.u16(voice + 0x5c) + length;
            m.w8(voice + 0x60, adjust + m.u8(voice + 0x5c));
        }
        std::uint32_t gate = 0x7fff;
        if ((m.u16(voice) & 0x600U) == 0) {
            const auto percent = m.u16(voice + 0x62);
            if (percent == 0xf) {
                gate = result - 1U;
            } else if (percent == 0x10) {
                gate = result;
            } else {
                gate = mul(u(s16(result)), percent) >> 4U;
            }
            if (percent != 0x10 && (gate & 0xffffU) == 0)
                gate = 1;
        }
        m.w32(voice + 0x5c, u(s16(result)) + (gate << 16U));
    }

    // 8003e290: the per-step change of a modulation of `depth` over `period`.
    static std::uint32_t lfo_step(std::uint32_t depth, std::uint32_t period, std::uint32_t shape) {
        if (depth == 0)
            return 0;
        const auto length = s16(period);
        const auto kind = s16(shape);
        if (length == 0 || kind < 2)
            return depth;
        if (kind < 4)
            return quotient(depth, u(length));
        if (kind == 4 && length != 1)
            return quotient(depth, u(length - 1));
        return depth;
    }
    // 8003e3e0: restart a modulation record.
    void lfo_reset(std::uint32_t lfo) {
        m.w16(lfo + 0x10, 1);
        m.w32(lfo + 4, 0);
        m.w16(lfo + 0x1e, m.u16(lfo + 0x1e) & 0xfff3U);
        m.w16(lfo + 0x14, m.u16(lfo + 0x16));
        m.w16(lfo + 0x18, m.u16(lfo + 0x1a));
    }
    // d8 (vibrato with a fixed shape), d9/e5/ed (vibrato, tremolo, pan with a
    // table shape): set up modulation record `index` of the voice.
    std::uint32_t lfo_setup(std::uint32_t at, std::uint32_t voice, std::uint32_t index, bool fixed,
                            bool squared, std::uint32_t shift) {
        const auto rate = m.u8(at);
        auto depth = u(s8(m.u8(at + 1)));
        const auto extra = m.u8(at + 2);
        if (depth == 0 || rate == 0)
            return at + 3;
        if (squared)
            depth = mul(depth, s32(depth) >= 0 ? depth : 0U - depth);
        auto square = mul(rate, rate);
        if (s32(square) < 0)
            square += 63;
        const auto period = (rate + sra(square, 6)) & 0xffffU;
        depth <<= shift;
        const auto lfo = voice + 0xd8 + 0x20 * index;
        const auto shape = fixed ? 3U : extra & 0xfU;
        m.w32(lfo + 0xc, lfo_step(depth, u(s16(period)), shape));
        m.w16(lfo + 0x1a, 0x400);
        m.w16(lfo + 0x12, period);
        if (fixed) {
            m.w32(lfo, 0x8003f2a0);
            m.w8(lfo + 0x1d, 3);
            m.w16(lfo + 0x1e, 3);
            m.w8(lfo + 0x1c, index);
            m.w16(lfo + 0x16, extra << 2U);
        } else {
            m.w16(lfo + 0x16, 0);
            m.w8(lfo + 0x1c, index);
            m.w8(lfo + 0x1d, shape);
            m.w16(lfo + 0x1e, ((extra & 0x10U) == 0 ? 2U : 0U) + 1U);
            m.w32(lfo, m.u32(0x800508a4 + shape * 4));
        }
        m.w16(voice + 0xce, m.u16(voice + 0xce) | (1U << index));
        lfo_reset(lfo);
        return at + 3;
    }
    // d7/e3/eb: the modulation depth fade-in step.
    std::uint32_t lfo_fade(std::uint32_t at, std::uint32_t voice, std::uint32_t index) {
        const auto steps = (m.u8(at) + 1U) & 0xffU;
        if (steps != 0) {
            const auto step = quotient(0x400, steps * 4);
            m.w16(voice + 0xf2 + 0x20 * index, step);
            m.w16(voice + 0xf0 + 0x20 * index, step);
        }
        return at + 1;
    }
    // db/e7/ef: stop modulation `index`.
    std::uint32_t lfo_stop(std::uint32_t at, std::uint32_t voice, std::uint32_t index) {
        m.w16(voice + 0xce, m.u16(voice + 0xce) & ~(1U << index) & 0xffffU);
        m.w16(voice + 0xf6 + 0x20 * index, m.u16(voice + 0xf6 + 0x20 * index) & 0xfffeU);
        return at;
    }
    std::uint32_t loop_frame(std::uint32_t voice) {
        return voice + m.u16(voice + 0x72) * 12 + 0x9c;
    }

    // One event opcode (table 80050624): returns the next event address.
    std::uint32_t opcode(std::uint32_t code, std::uint32_t at, std::uint32_t seq,
                         std::uint32_t voice) {
        const auto handler = m.u32(0x80050624 + (code - 0x80) * 4);
        switch (handler) {
        case 0x8003cd08: // 80 rest
            m.w16(voice + 0x5c, m.u8(at));
            m.w16(voice + 2, m.u16(voice + 2) | 2U);
            m.w16(voice, m.u16(voice) | 0x400U);
            return at + 1;
        case 0x8003cd30: // 81 tie
            m.w16(voice, m.u16(voice) | 0x100U);
            m.w16(voice + 0x5c, m.u8(at));
            return at + 1;
        case 0x8003cd8c: // 90 end or loop back
            if (const auto loop = m.u32(voice + 0x18); loop != 0) {
                m.w16(voice + 0x20, m.u16(voice + 0x20) + 1);
                m.w16(voice + 0x66, m.u8(voice + 0x23));
                return loop;
            }
            m.w16(voice + 2, m.u16(voice + 2) & 0xfffcU);
            release_voice(m.d, voice + 0x30, m.u8(voice + 0x27));
            m.w16(voice, 0);
            return at;
        case 0x8003ce04: // 91 loop point
            m.w32(voice + 0x18, at);
            m.w8(voice + 0x23, m.u8(voice + 0x66));
            return at;
        case 0x8003ce18: // 94 octave
            m.w16(voice + 0x66, m.u8(at) * 12);
            return at + 1;
        case 0x8003ce38: // 95 octave up
            m.w16(voice + 0x66, m.u16(voice + 0x66) + 12);
            return at;
        case 0x8003ce50: // 96 octave down
            m.w16(voice + 0x66, m.u16(voice + 0x66) - 12);
            return at;
        case 0x8003ce68: { // 97 time signature
            const auto divisor = m.u8(at + 1);
            const auto ticks = quotient(0xc0, divisor);
            m.w16(seq + 0x3a, ticks);
            m.w16(seq + 0x3c, divisor);
            m.w16(seq + 0x38, m.u8(at));
            m.w16(seq + 0x3e, m.u8(at));
            m.w16(seq + 0x36, m.u16(seq + 0x3a));
            return at + 2;
        }
        case 0x8003cef0: { // 98 repeat start
            m.w16(voice + 0x72, m.u16(voice + 0x72) + 1);
            const auto frame = loop_frame(voice);
            m.w8(frame, m.u8(at) + 0xff);
            m.w32(frame + 4, at + 1);
            m.w8(frame + 2, m.u8(voice + 0x66));
            return at + 1;
        }
        case 0x8003cf38: { // 99 repeat end
            const auto frame = loop_frame(voice);
            const auto left = (m.u8(frame) - 1U) & 0xffU;
            m.w8(frame, left);
            if (left == 0xff) {
                m.w16(voice + 0x72, m.u16(voice + 0x72) - 1);
                return at;
            }
            m.w32(frame + 8, at);
            m.w8(frame + 3, m.u8(voice + 0x66));
            m.w16(voice + 0x66, m.u8(frame + 2));
            return m.u32(frame + 4);
        }
        case 0x8003cfa4: { // 9a repeat break on the last pass
            const auto frame = loop_frame(voice);
            if (m.u8(frame) != 0)
                return at;
            const auto next = m.u32(frame + 8);
            m.w16(voice + 0x66, m.u8(frame + 3));
            m.w16(voice + 0x72, m.u16(voice + 0x72) - 1);
            return next;
        }
        case 0x8003d0e8: { // a0 tempo
            const auto tempo = m.u8(at);
            m.w32(seq + 0x58, tempo << 16U);
            m.w32(seq + 0x54, mul(tempo, u(s16(m.u16(seq + 0x66)))));
            return at + 1;
        }
        case 0x8003d13c: { // a2 tempo slide
            const auto steps = m.u8(at);
            const auto target = m.u8(at + 1);
            m.w16(seq + 0x62, target);
            const auto distance = (target << 16U) - m.u32(seq + 0x58);
            if (steps != 0 && distance != 0) {
                m.w32(seq + 0x5c, quotient(distance, steps));
                m.w16(seq + 0x60, steps);
            }
            return at + 2;
        }
        case 0x8003d208: // a9 gate percentage
            m.w16(voice + 0x62, m.u8(at));
            return at + 1;
        case 0x8003d21c: { // aa hardware voice
            const auto channel = m.u8(at);
            if (channel < 25) {
                release_voice(m.d, voice + 0x30, m.u8(voice + 0x27));
                m.w8(voice + 0x27, channel);
                claim_voice(m.d, voice + 0x30, channel);
            }
            return at + 1;
        }
        case 0x8003d298: // ac instrument
            load_instrument(m.d, m.u8(at), voice);
            return at + 1;
        case 0x8003d2d0: { // ad gate adjustment
            const auto value = m.u8(at);
            m.w8(voice + 0x60, value != 0 ? value + m.u8(voice + 0x60) : 0U);
            return at + 1;
        }
        case 0x8003d300: // ae drum kit on
            if (m.u32(seq + 0xc) != 0)
                m.w16(voice, m.u16(voice) | 0x10U);
            return at;
        case 0x8003d340: // b0
            m.w16(voice, m.u16(voice) | 0x800U);
            return at;
        case 0x8003d370: // b2 frequency modulation on
        case 0x8003d3a4: // b3 off
            if ((m.u8(voice + 0x27) & 1U) != 0) {
                m.w16(voice + 0x36, m.u16(voice + 0x36) | 0x1000U);
                m.w16(voice + 0x32, handler == 0x8003d370 ? m.u16(voice + 0x32) | 0x10U
                                                          : m.u16(voice + 0x32) & 0xffefU);
            }
            return at;
        case 0x8003d3d8:   // b4 noise clock
        case 0x8003d438: { // b5 noise clock change
            const auto value =
                handler == 0x8003d3d8 ? m.u8(at) : (m.u8(at) + m.u16(seq + 0x1c)) & 0x3fU;
            m.w16(seq + 0x1c, value);
            noise_clock(m.u16(seq + 0x1c));
            m.w16(voice + 0x36, m.u16(voice + 0x36) | 0x2000U);
            m.w16(voice + 0x32, m.u16(voice + 0x32) | 0x20U);
            return at + 1;
        }
        case 0x8003d53c: // ba reverb on, unless the reverb is reserved
            if ((m.u16(seq + 0x10) & 6U) != 0 &&
                ((m.u16(0x8005957c) & 0x2000U) == 0 || (m.u16(voice) & 2U) != 0))
                return at;
            m.w16(voice + 0x36, m.u16(voice + 0x36) | 0x4000U);
            m.w16(voice + 0x32, m.u16(voice + 0x32) | 0x40U);
            return at;
        case 0x8003d59c: // bb reverb off
            m.w16(voice + 0x36, m.u16(voice + 0x36) | 0x4000U);
            m.w16(voice + 0x32, m.u16(voice + 0x32) & 0xffbfU);
            return at;
        case 0x8003d5d4: // c0 reload the instrument
            load_instrument(m.d, m.u8(voice + 0x26), voice);
            return at;
        case 0x8003d640: // c2 envelope byte 57
            m.w16(voice + 0x36, m.u16(voice + 0x36) | 0x10U);
            m.w8(voice + 0x57, m.u8(at));
            return at + 1;
        case 0x8003d678: // c4 envelope byte 59
            m.w16(voice + 0x36, m.u16(voice + 0x36) | 0x40U);
            m.w8(voice + 0x59, m.u8(at));
            return at + 1;
        case 0x8003d6d0: // c7 envelope bytes 58 and 5b
            m.w8(voice + 0x58, m.u8(at));
            m.w16(voice + 0x36, m.u16(voice + 0x36) | 0x120U);
            m.w8(voice + 0x5b, m.u8(at + 1));
            return at + 2;
        case 0x8003d714: // c9 envelope byte 55
            m.w16(voice + 0x36, m.u16(voice + 0x36) | 0x40U);
            m.w8(voice + 0x55, m.u8(at));
            return at + 1;
        case 0x8003d74c: // d0 detune
            m.w16(voice + 0x6e, u(s8(m.u8(at))) << 5U);
            m.w16(voice + 2, m.u16(voice + 2) | 0x200U);
            return at + 1;
        case 0x8003d770: // d1 detune change
            m.w16(voice + 0x6e, m.u16(voice + 0x6e) + (u(s8(m.u8(at))) << 5U));
            m.w16(voice + 2, m.u16(voice + 2) | 0x200U);
            return at + 1;
        case 0x8003d79c: // d2 detune change in steps of 8
            m.w16(voice + 0x6e, m.u16(voice + 0x6e) + (u(s8(m.u8(at))) << 3U));
            m.w16(voice + 2, m.u16(voice + 2) | 0x200U);
            return at + 1;
        case 0x8003d7c8: { // d3 detune change by a halfword (signed high byte first)
            const auto change = (u(s8(m.u8(at))) << 8U) + m.u8(at + 1);
            m.w16(voice + 2, m.u16(voice + 2) | 0x200U);
            m.w16(voice + 0x6e, m.u16(voice + 0x6e) + change);
            return at + 2;
        }
        case 0x8003d7fc: { // d4 pitch slide
            const auto steps = m.u8(at);
            const auto distance = u(s8(m.u8(at + 1))) << 24U;
            if (steps != 0 && distance != 0) {
                const auto step = quotient(distance, steps);
                m.w16(voice + 0x94, steps);
                m.w16(voice + 4, m.u16(voice + 4) | 1U);
                m.w32(voice + 0x84, step);
            } else {
                m.w16(voice + 4, m.u16(voice + 4) & 0xfffeU);
            }
            return at + 2;
        }
        case 0x8003dab0: // d7 vibrato fade
            return lfo_fade(at, voice, 0);
        case 0x8003d8b8: // d8 vibrato (fixed shape)
            return lfo_setup(at, voice, 0, true, true, 14);
        case 0x8003d9a4: // d9 vibrato
            return lfo_setup(at, voice, 0, false, true, 14);
        case 0x8003db0c: // db vibrato off
            return lfo_stop(at, voice, 0);
        case 0x8003db2c: // e0 volume
            m.w32(voice + 0x78, m.u8(at) << 24U);
            m.w16(voice + 4, m.u16(voice + 4) & 0xfef7U);
            m.w16(voice + 2, m.u16(voice + 2) | 0x100U);
            return at + 1;
        case 0x8003db58: // e1 volume change
            m.w32(voice + 0x78, ((u(s8(m.u8(at))) << 24U) + m.u32(voice + 0x78)) & 0x7fffffffU);
            m.w16(voice + 2, m.u16(voice + 2) | 0x100U);
            m.w16(voice + 4, m.u16(voice + 4) & 0xfef7U);
            return at + 1;
        case 0x8003db98: { // e2 volume slide
            const auto steps = m.u8(at);
            const auto distance = (u(s8(m.u8(at + 1))) << 24U) - m.u32(voice + 0x78);
            if (steps != 0 && distance != 0) {
                const auto step = quotient(distance, steps);
                m.w16(voice + 4, (m.u16(voice + 4) | 8U) & 0xfeffU);
                m.w16(voice + 0x96, steps);
                m.w32(voice + 0x88, step);
            }
            return at + 2;
        }
        case 0x8003de18: // e3 tremolo fade
            return lfo_fade(at, voice, 1);
        case 0x8003dd24: // e5 tremolo
            return lfo_setup(at, voice, 1, false, false, 24);
        case 0x8003de74: // e7 tremolo off
            return lfo_stop(at, voice, 1);
        case 0x8003de94: // e8 pan
            m.w16(voice + 0x74, m.u8(at) << 8U);
            m.w16(voice + 2, m.u16(voice + 2) | 0x100U);
            return at + 1;
        case 0x8003df3c: // eb pan modulation fade
            return lfo_fade(at, voice, 2);
        case 0x8003e04c: // ed pan modulation
            return lfo_setup(at, voice, 2, false, false, 24);
        case 0x8003e160: // ef pan modulation off
            return lfo_stop(at, voice, 2);
        case 0x8003e44c: { // fc wave bank and instrument
            const auto bank = m.u8(at);
            const auto instrument = m.u8(at + 1);
            m.w8(voice + 0x25, bank);
            auto wave = find_wave_bank(m.d, bank);
            if (wave == 0)
                wave = m.d.wave_banks;
            m.w32(voice + 0x2c, wave);
            load_instrument(m.d, instrument, voice);
            return at + 2;
        }
        default:
            missing("sound_opcode", handler);
        }
    }

    // 8004d364: the noise clock (bits 8-13 of the SPU control register).
    void noise_clock(std::uint32_t value) {
        auto clock = s32(value) < 0 ? 0 : s32(value) < 64 ? value : 63U;
        const auto control = m.d.spu_registers + 0x1aa;
        spu_write(control, (spu_read(0x8004d394, 2) & 0xc0ffU) | ((clock & 0x3fU) << 8U));
    }

    // 8003efe4: step each active modulation and add it to the voice's pitch,
    // volume or pan offset.
    void modulate(std::uint32_t voices, std::uint32_t count) {
        for (auto voice = voices; count != 0; --count, voice += voice_stride) {
            if (m.u16(voice) == 0)
                continue;
            const auto active = m.u16(voice + 0xce);
            m.w16(voice + 0xd4, 0);
            m.w16(voice + 0xd2, 0);
            m.w16(voice + 0xd0, 0);
            if (active == 0)
                continue;
            auto changes = m.u16(voice + 2);
            for (std::uint32_t lfo = voice + 0xd8; lfo < voice + 0x158; lfo += 0x20) {
                if ((m.u16(lfo + 0x1e) & 1U) == 0)
                    continue;
                if (const auto delay = m.u16(lfo + 0x14); delay != 0) {
                    m.w16(lfo + 0x14, delay - 1);
                    continue;
                }
                auto value = shape(lfo);
                const auto fade = u(s16(m.u16(lfo + 0x18)));
                if (s32(fade) < 0x400) {
                    m.w16(lfo + 0x18, fade + m.u16(lfo + 0x1a));
                    value = mul(sra(value, 10), fade);
                }
                value = sra(value, 16);
                switch (m.u8(lfo + 0x1c)) {
                case 0:
                    m.w16(voice + 0xd0, m.u16(voice + 0xd0) + value);
                    changes |= 0x200U;
                    break;
                case 1:
                    m.w16(voice + 0xd2, m.u16(voice + 0xd2) + value);
                    changes |= 0x100U;
                    break;
                case 2:
                    m.w16(voice + 0xd4, m.u16(voice + 0xd4) + value);
                    changes |= 0x100U;
                    break;
                default:
                    break;
                }
            }
            m.w16(voice + 2, changes);
        }
    }

    // 8003f43c: the modulation noise generator.
    std::uint32_t random() {
        auto value = m.u32(noise);
        value ^= value << 17U;
        value ^= sra(value, 15);
        m.w32(noise, value);
        return value & 0x7fffU;
    }

    // Modulation shapes (table 800508a4).
    std::uint32_t shape(std::uint32_t lfo) {
        const auto handler = m.u32(lfo);
        const auto step = [&] {
            const auto left = (m.u16(lfo + 0x10) - 1U) & 0xffffU;
            m.w16(lfo + 0x10, left);
            if (left == 0)
                m.w16(lfo + 0x10, m.u16(lfo + 0x12));
            return left == 0;
        };
        const auto flags = [&] { return m.u16(lfo + 0x1e); };
        switch (handler) {
        case 0x8003f190: // Off: clear the active bit.
            m.w16(lfo + 0x1e, flags() & 0xfffeU);
            return flags();
        case 0x8003f1a4: // Square: toggle between 0 and the depth.
            if (step())
                m.w32(lfo + 4, m.u32(lfo + 4) == 0 ? m.u32(lfo + 0xc) : 0U);
            return m.u32(lfo + 4);
        case 0x8003f1ec: // Square around zero.
            if (step()) {
                const auto depth = m.u32(lfo + 0xc);
                m.w32(lfo + 4, (flags() & 8U) != 0 ? 0U - depth : depth);
                m.w16(lfo + 0x1e, flags() ^ 8U);
            }
            return m.u32(lfo + 4);
        case 0x8003f240: // Triangle.
            if (step()) {
                const auto depth = m.u32(lfo + 0xc);
                m.w32(lfo + 8, (flags() & 8U) != 0 ? 0U - depth : depth);
                m.w16(lfo + 0x1e, flags() ^ 8U);
            }
            m.w32(lfo + 4, m.u32(lfo + 4) + m.u32(lfo + 8));
            return m.u32(lfo + 4);
        case 0x8003f2a0: { // Triangle from the center (the first half period).
            auto left = (m.u16(lfo + 0x10) - 1U) & 0xffffU;
            if (left == 0) {
                const auto state = flags();
                left = m.u16(lfo + 0x12);
                if ((state & 4U) != 0)
                    left = (left << 1U) & 0xffffU;
                const auto depth = m.u32(lfo + 0xc);
                m.w32(lfo + 8, (state & 8U) != 0 ? 0U - depth : depth);
                m.w16(lfo + 0x1e, (state | 4U) ^ 8U);
            }
            m.w16(lfo + 0x10, left);
            m.w32(lfo + 4, m.u32(lfo + 4) + m.u32(lfo + 8));
            return m.u32(lfo + 4);
        }
        case 0x8003f308: // Saw.
            if (step())
                m.w32(lfo + 4, 0);
            else
                m.w32(lfo + 4, m.u32(lfo + 4) + m.u32(lfo + 0xc));
            return m.u32(lfo + 4);
        case 0x8003f354: // Random.
            static_cast<void>(random());
            if (step())
                m.w32(lfo + 4, mul(sra(m.u32(lfo + 0xc), 15), random()));
            return m.u32(lfo + 4);
        case 0x8003f3c0: // Random around zero.
            if (step()) {
                const auto depth = m.u32(lfo + 0xc);
                m.w32(lfo + 4, mul(sra(depth, 14), random()) - depth);
            }
            return m.u32(lfo + 4);
        default:
            missing("sound_modulation_shape", handler);
        }
    }
};
} // namespace

std::uint32_t Program::sound_tick(std::uint32_t event) { return Tick(resident).run(event); }

// 8003bc10: queue an SPU write transfer (type 1) of `size` bytes from RAM
// `ram` to SPU address `spu`. Outside the transfer callback 8003bca0 first
// waits while the queue is full; only the SPU DMA interrupt drains it.
void Program::spu_transfer(std::uint32_t spu, std::uint32_t ram, std::uint32_t size,
                           std::uint32_t callback) {
    using Kind = PlatformInput::Kind;
    if ((resident.sound.flags & 4U) == 0) {
        while (Tick(resident).queue_full())
            if (!deliver_interrupt())
                throw MissingDependency({"spu_transfer_wait", 0x8003bce4, {}, {}},
                                        "interrupt:spu-transfer-completion", false,
                                        "Waiting for a free SPU transfer entry needs the "
                                        "interrupt arrivals that drain the queue");
        // Starting the transfer reads the SPU control register (8004cd8c);
        // the arrivals recorded before that read precede it.
        if ((resident.sound.flags & 0x10U) == 0)
            while (!resident.platform.empty() &&
                   (resident.platform.front().kind == Kind::interrupt ||
                    resident.platform.front().kind == Kind::tick))
                static_cast<void>(deliver_interrupt());
    }
    Tick(resident).enqueue(1, spu, ram, size, callback);
}

// 8003bdfc: with flag 10, wait until no SPU transfer is in progress; then
// the type of the transfer in progress, or zero.
std::int32_t Program::sound_wait(std::uint32_t flags) {
    auto &driver = resident.sound;
    if ((flags & 0x10U) != 0)
        while ((driver.flags & 0x10U) != 0)
            if (!deliver_interrupt())
                throw MissingDependency({"sound_wait", 0x8003be08, {}, {}},
                                        "interrupt:spu-transfer-completion", false,
                                        "Waiting for the SPU transfers needs the interrupt "
                                        "arrivals that complete them");
    if ((driver.flags & 0x10U) == 0)
        return 0;
    Memory m(driver);
    return s16(m.u16(m.u32(0x80059458) + m.u16(0x80059510) * 20U));
}

// 8004e574 SpuSetReverbDepth.
void Program::spu_reverb_depth(std::uint16_t left, std::uint16_t right) {
    Tick(resident).reverb_depth(left, right);
}

void Program::spu_transfer_completed() { Tick(resident).transfer_completed(); }

} // namespace xem::reconstruction
