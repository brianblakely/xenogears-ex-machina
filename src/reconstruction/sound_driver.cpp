#include "xem/reconstruction/sound_driver.hpp"

#include <algorithm>
#include <iterator>
#include <limits>
#include <optional>

namespace xem::reconstruction::resident {
namespace {

// Bytes at an original address inside one owned object.
std::uint8_t *at(SoundDriver &driver, std::uint32_t address, std::uint32_t size) {
    auto found = driver.objects.upper_bound(address);
    if (found != driver.objects.begin()) {
        --found;
        if (address - found->first + std::uint64_t{size} <= found->second.size())
            return found->second.data() + (address - found->first);
    }
    throw SoundError("Sound driver reaches memory outside its owned objects");
}
std::uint32_t get(SoundDriver &driver, std::uint32_t address, std::uint32_t size) {
    const auto *bytes = at(driver, address, size);
    std::uint32_t value = 0;
    for (std::uint32_t i = 0; i < size; ++i)
        value |= static_cast<std::uint32_t>(bytes[i]) << (8U * i);
    return value;
}
void put(SoundDriver &driver, std::uint32_t address, std::uint32_t value, std::uint32_t size) {
    auto *bytes = at(driver, address, size);
    for (std::uint32_t i = 0; i < size; ++i)
        bytes[i] = static_cast<std::uint8_t>(value >> (8U * i));
}
std::uint32_t u8(SoundDriver &d, std::uint32_t a) { return get(d, a, 1); }
std::uint32_t u16(SoundDriver &d, std::uint32_t a) { return get(d, a, 2); }
std::uint32_t u32(SoundDriver &d, std::uint32_t a) { return get(d, a, 4); }
void put8(SoundDriver &d, std::uint32_t a, std::uint32_t v) { put(d, a, v, 1); }
void put16(SoundDriver &d, std::uint32_t a, std::uint32_t v) { put(d, a, v, 2); }
void put32(SoundDriver &d, std::uint32_t a, std::uint32_t v) { put(d, a, v, 4); }
std::int32_t s16(std::uint32_t value) { return static_cast<std::int16_t>(value); }
std::uint32_t bit(std::uint32_t shift) { return 1U << (shift & 31U); }

std::uint32_t voice(SoundDriver &driver, std::uint32_t index) {
    return driver.effect_block + voice_records + index * voice_stride;
}

} // namespace

// 8003e83c: release hardware voice `channel` if `owner` holds it.
void release_voice(SoundDriver &driver, std::uint32_t owner, std::uint32_t channel) {
    if (channel >= driver.voice_owners.size() || driver.voice_owners[channel] != owner)
        return;
    driver.voice_owners[channel] = 0;
    driver.voice_changes |= bit(channel);
    driver.voice_holds &= ~bit(channel);
}

// 8003e724: claim hardware voice `channel` unless a holder has a higher
// priority (owner +4, signed).
void claim_voice(SoundDriver &driver, std::uint32_t owner, std::uint32_t channel) {
    if (channel >= driver.voice_owners.size())
        return;
    const auto holder = driver.voice_owners[channel];
    if (holder == owner) {
        driver.voice_changes |= bit(channel);
        return;
    }
    if (holder != 0 && s16(u16(driver, owner + 4)) < s16(u16(driver, holder + 4)))
        return;
    put16(driver, owner + 6, 0xffff);
    put16(driver, owner, channel);
    driver.voice_owners[channel] = owner;
    driver.voice_changes |= bit(channel);
    driver.voice_holds &= ~bit(channel);
}

// 8003e5bc: load instrument `instrument` of the voice's wave bank (+2c).
void load_instrument(SoundDriver &driver, std::uint32_t instrument, std::uint32_t record) {
    put8(driver, record + 0x26, instrument);
    const auto wave = u32(driver, record + 0x2c);
    const auto entry =
        wave + 0x30 + static_cast<std::uint32_t>(static_cast<std::int32_t>(instrument << 16) >> 12);
    const auto start = u32(driver, entry) << 3;
    put32(driver, record + 0x4c, start + u32(driver, wave + 0x28));
    put32(driver, record + 0x50, start + (u16(driver, entry + 4) << 3));
    const auto envelope = u16(driver, entry + 0xc);
    put8(driver, record + 0x54, envelope & 7);
    put8(driver, record + 0x55, (envelope >> 4) & 7);
    put8(driver, record + 0x56, (envelope >> 8) & 7);
    const auto rates = u32(driver, entry + 8);
    put8(driver, record + 0x57, rates & 0x7f);
    put8(driver, record + 0x58, (rates >> 8) & 0xf);
    put8(driver, record + 0x59, (rates >> 16) & 0x7f);
    put8(driver, record + 0x5b, (rates >> 12) & 0xf);
    put8(driver, record + 0x28, (rates >> 24) & 0x1f);
    put8(driver, record + 0x5a, (rates >> 24) & 0x1f);
    put16(driver, record, u16(driver, record) | 0x8000);
    put16(driver, record + 0x6c, u16(driver, entry + 6));
}

// 800383ec: the wave bank with `id`, or zero.
std::uint32_t find_wave_bank(SoundDriver &driver, std::uint32_t id) {
    auto bank = driver.wave_banks;
    while (bank != 0 && u16(driver, bank + 0x20) != id)
        bank = u32(driver, bank + 0x2c);
    return bank;
}

namespace {

// 8003b644: initialize `effect_run` voices from voice (code & ff) with effect
// `id`. The original brackets the setup with BIOS DisableEvent/EnableEvent on
// the driver's event (800595bc); that critical section holds no driver state.
void start_effect_voices(SoundDriver &driver, std::uint32_t code, std::uint32_t id,
                         std::int32_t volume, std::int32_t pan) {
    auto bank = driver.effect_banks;
    while (static_cast<std::int32_t>(u16(driver, bank + 0x14)) !=
           static_cast<std::int32_t>(id) >> 16) {
        bank = u32(driver, bank + 0x1c);
        if (bank == 0)
            return;
    }
    const auto effects = driver.effect_block;
    auto wave = find_wave_bank(driver, u16(driver, bank + 0x16));
    if (wave == 0)
        wave = driver.wave_banks;
    const auto effect = id & 0xffff;
    const auto scale = u8(driver, bank + u16(driver, bank + 0x18) + effect);
    auto level = static_cast<std::uint32_t>(volume * static_cast<std::int32_t>(scale)) >> 7;
    if ((level >> 15) & 1)
        level = 0x7fff;
    auto offsets = bank + effect * 4 + 0x20;
    auto record = voice(driver, code & 0xff);
    if (driver.effect_run == 0)
        throw SoundError("A zero effect voice count loops through the whole counter");
    for (auto remaining = driver.effect_run; remaining != 0; --remaining) {
        put32(driver, record + 8, id);
        put32(driver, record + 0xc, driver.start_stamp);
        put8(driver, record + 7, (code >> 8) & 0xff);
        const auto data = u16(driver, offsets);
        const auto active = bit(u8(driver, record + 6));
        if (data == 0) {
            put32(driver, effects + 0x48, u32(driver, effects + 0x48) & ~active);
            put16(driver, record, 0);
            release_voice(driver, record + 0x30, u8(driver, record + 0x27));
        } else {
            put32(driver, effects + 0x48, u32(driver, effects + 0x48) | active);
            put16(driver, record, (u16(driver, bank + 0x10) & 1) != 0 ? 0x40b : 0x409);
            put16(driver, record + 2, 0x170);
            put16(driver, record + 4, 0);
            put16(driver, record + 0x66, 0x3c);
            put16(driver, record + 0x62, 0xf);
            put16(driver, record + 0x72, 0xffff);
            put32(driver, record + 0x18, 0);
            put32(driver, record + 0x1c, 0);
            put16(driver, record + 0x20, 0);
            put8(driver, record + 0x22, 0);
            put16(driver, record + 0x5c, 0);
            put8(driver, record + 0x60, 0);
            put16(driver, record + 0x6e, 0);
            put8(driver, record + 0x64, 0);
            put16(driver, record + 0x76, level);
            put32(driver, record + 0x78, 0x7f000000);
            for (const auto offset : {0x70U, 0xd0U, 0xd2U, 0xd4U, 0x3cU, 0x3eU, 0xceU})
                put16(driver, record + offset, 0);
            put16(driver, record + 0x74, static_cast<std::uint32_t>(pan));
            put32(driver, record + 0x10, bank + data);
            put32(driver, record + 0x14, bank + data);
            for (std::uint32_t slot = 0; slot < 4; ++slot)
                put16(driver, record + 0xf6 + 0x20 * slot, 0);
            put8(driver, record + 0x25, u8(driver, bank + 0x16));
            put32(driver, record + 0x2c, wave);
            if (wave != 0)
                load_instrument(driver, 0, record);
            put16(driver, record + 0x32, 0);
            put16(driver, record + 0x34, 0x200);
            claim_voice(driver, record + 0x30, u8(driver, record + 0x27));
        }
        offsets += 2;
        record += voice_stride;
    }
    put16(driver, effects + 0x10, u16(driver, effects + 0x10) | 0x8000);
}

// 8003b060: release the hardware voice of each of the sequence's voices.
void release_sequence_voices(SoundDriver &driver, std::uint32_t sequence) {
    auto count = u8(driver, sequence + 0x14);
    if (count == 0)
        throw SoundError("A sequence without voices loops through the whole counter");
    for (auto voice = sequence + voice_records; count != 0; --count, voice += voice_stride)
        release_voice(driver, voice + 0x30, u8(driver, voice + 0x27));
}

// 800396e0: free the SPU allocation holding `address`; returns it, or zero
// when no entry holds it.
std::uint32_t free_spu_block(SoundDriver &driver, std::uint32_t address) {
    auto &table = driver.spu_blocks;
    const auto field = [&](std::uint32_t entry, std::uint32_t offset, std::uint32_t size) {
        if (entry >= table.size() / 16)
            throw SoundError("SPU allocation list leaves its table");
        std::uint32_t value = 0;
        for (std::uint32_t i = 0; i < size; ++i)
            value |= static_cast<std::uint32_t>(table[entry * 16 + offset + i]) << (8U * i);
        return value;
    };
    const auto store = [&](std::uint32_t entry, std::uint32_t offset, std::uint32_t value,
                           std::uint32_t size) {
        for (std::uint32_t i = 0; i < size; ++i)
            table[entry * 16 + offset + i] = static_cast<std::uint8_t>(value >> (8U * i));
    };
    std::optional<std::uint32_t> previous;
    for (std::uint32_t entry = 0, visited = 0;; ++visited) {
        if (visited == table.size() / 16)
            throw SoundError("SPU allocation list does not terminate");
        if (field(entry, 4, 4) == address) {
            if (!previous)
                throw SoundError("Freeing the SPU table head writes through a null entry");
            store(*previous, 2, field(entry, 2, 2), 2);
            store(entry, 0, 0, 1);
            store(entry, 1, 0, 1);
            store(entry, 4, 0, 4);
            store(entry, 2, 0, 2);
            return address;
        }
        const auto next = static_cast<std::int16_t>(field(entry, 2, 2));
        if (next == 0)
            return 0;
        previous = entry;
        entry = static_cast<std::uint32_t>(next);
    }
}

// 8003ba38: unlink a sequence from the sequence list, stopping it if playing.
void unlink_sequence(SoundDriver &driver, std::uint32_t sequence) {
    std::uint32_t previous = 0;
    auto at = driver.sequences;
    while (at != sequence) {
        if (at == 0)
            throw SoundError("Released sequence is not on the sequence list");
        previous = at;
        at = u32(driver, previous);
    }
    if (const auto flags = u16(driver, sequence + 0x10); (flags & 0x8000) != 0) {
        put16(driver, sequence + 0x10, flags & 0x7fff);
        release_sequence_voices(driver, sequence);
    }
    if (previous != 0)
        put32(driver, previous, u32(driver, sequence));
    else
        driver.sequences = u32(driver, sequence);
}

} // namespace

// 80039144: unlink a pool block from the allocated list between BIOS
// DisableEvent/EnableEvent. The original skips the unlink when the block is
// the list head.
void free_pool_block(SoundDriver &driver, std::uint32_t object) {
    const auto header = object - 0x10;
    if (driver.pool == header)
        return;
    auto previous = driver.pool;
    for (;;) {
        const auto found = driver.pool_headers.find(previous);
        if (found == driver.pool_headers.end())
            throw SoundError("Sound pool list leaves its owned headers");
        if (found->second[3] == header)
            break;
        previous = found->second[3];
    }
    const auto freed = driver.pool_headers.find(header);
    if (freed == driver.pool_headers.end())
        throw SoundError("Freed sound pool block has no owned header");
    driver.pool_headers.at(previous)[3] = freed->second[3];
}

void set_effect_pair(SoundDriver &driver, std::uint32_t channel, std::uint32_t field,
                     std::uint32_t value) {
    const auto first = (channel & 0xfe) ^ 8;
    for (auto index = first; index < first + 2; ++index) {
        const auto record = voice(driver, index);
        if ((u16(driver, record) & 1) == 0)
            continue;
        put16(driver, record + field, value << 8);
        put16(driver, record + 2, 0x100);
    }
}

void stop_effect_pair(SoundDriver &driver, std::uint32_t channel) {
    const auto first = (channel & 0xfe) ^ 8;
    for (auto index = first; index < first + 2; ++index) {
        const auto record = voice(driver, index);
        if ((u16(driver, record) & 1) == 0)
            continue;
        put16(driver, record, 0);
        const auto effects = driver.effect_block;
        put32(driver, effects + 0x48, u32(driver, effects + 0x48) & ~bit(u8(driver, record + 6)));
        release_voice(driver, record + 0x30, u8(driver, record + 0x27));
    }
}

void release_effect_bank(SoundDriver &driver, std::uint32_t bank) {
    std::uint32_t previous = 0;
    auto at = driver.effect_banks;
    while (at != 0 && at != bank) {
        previous = at;
        at = u32(driver, at + 0x1c);
    }
    if (at == 0)
        throw SoundError("Released effect bank is not on the effect bank list (driver error 0x10)");
    // 8003a094: stop the effect voices playing an effect of this bank.
    const auto id = s16(u16(driver, bank + 0x14));
    for (std::uint32_t index = 0; index < driver.voice_limit; ++index) {
        const auto record = voice(driver, index);
        if ((u16(driver, record) & 1) == 0 || s16(u16(driver, record + 0xa)) != id)
            continue;
        put16(driver, record, 0);
        const auto effects = driver.effect_block;
        put32(driver, effects + 0x48, u32(driver, effects + 0x48) & ~bit(u8(driver, record + 6)));
        release_voice(driver, record + 0x30, u8(driver, record + 0x27));
    }
    // Between BIOS DisableEvent/EnableEvent.
    if (previous != 0)
        put32(driver, previous + 0x1c, u32(driver, bank + 0x1c));
    else
        driver.effect_banks = u32(driver, bank + 0x1c);
    put32(driver, bank + 0x1c, 0);
    // 8003f614: the bank's signature 'sesd', a zero word sum over its
    // length (+8) and version 101.
    std::uint32_t sum = 0;
    for (std::uint32_t word = 0; word < (u32(driver, bank + 8) + 3) / 4; ++word)
        sum += u32(driver, bank + 4 * word);
    if (u32(driver, bank) != 0x73646573 || sum != 0 || u16(driver, bank + 0xc) != 0x101)
        throw SoundError("Released effect bank fails its signature check (driver error 0xb)");
}

void start_effect(SoundDriver &driver, std::uint32_t id, std::uint32_t channel,
                  std::uint32_t volume, std::uint32_t pan) {
    if ((driver.flags & 0x800) == 0)
        return;
    driver.effect_run = 2;
    // The byte arguments are sign-extended and shifted left 8 (sll 24, sra 16).
    const auto widen = [](std::uint32_t value) {
        return static_cast<std::int32_t>(static_cast<std::int8_t>(value & 0xff)) * 256;
    };
    start_effect_voices(driver, ((channel & 0xfe) ^ 8) | 0x2000, id, widen(volume), widen(pan));
}

void start_bank_effect(SoundDriver &driver, std::uint32_t bank, std::uint32_t effect) {
    if ((driver.flags & 0x800) == 0)
        return;
    driver.effect_run = 2;
    // (limit - 2) | ffff8000, sign-extended from 16 bits.
    const auto code = static_cast<std::uint32_t>(
        static_cast<std::int16_t>(static_cast<std::uint16_t>((driver.voice_limit - 2) | 0x8000)));
    start_effect_voices(driver, code, u16(driver, bank + 0x14) << 16 | (effect & 0xff), 0x6000,
                        0x4000);
}

void stop_sequence(SoundDriver &driver, std::uint32_t sequence) {
    if (sequence == 0)
        throw SoundError("Stopping a null sequence reaches the driver error handler 8003f6b0");
    put16(driver, sequence + 0x10, u16(driver, sequence + 0x10) & 0x7fff);
    release_sequence_voices(driver, sequence);
}

void release_sequence(SoundDriver &driver, std::uint32_t sequence) {
    if ((u16(driver, sequence + 0x10) & 0x8000) != 0)
        stop_sequence(driver, sequence);
    // 8003f67c (the sequence's +8 handle check) always returns zero.
    unlink_sequence(driver, sequence);
    // 8003b930: free the child block chain (next +4).
    if (auto child = u32(driver, sequence + 4); child != 0) {
        put32(driver, sequence + 4, 0);
        while (child != 0) {
            const auto next = u32(driver, child + 4);
            free_pool_block(driver, child);
            child = next;
        }
    }
    if ((u16(driver, sequence + 0x10) & 0x4000) == 0)
        free_pool_block(driver, sequence);
}

void release_wave_bank(SoundDriver &driver, std::uint32_t wave) {
    // The list walk ends at a null link, so a null wave bank is never found.
    std::uint32_t previous = 0;
    for (auto at = driver.wave_banks; at != wave || wave == 0; at = u32(driver, at + 0x2c)) {
        if (at == 0)
            throw SoundError("Released wave bank is not on the wave bank list (driver error 0x11)");
        previous = at;
    }
    // Between BIOS DisableEvent/EnableEvent.
    if (previous != 0)
        put32(driver, previous + 0x2c, u32(driver, wave + 0x2c));
    else
        driver.wave_banks = u32(driver, wave + 0x2c);
    const auto spu = u32(driver, wave + 0x28);
    if (free_spu_block(driver, spu) != spu)
        throw SoundError("Released wave bank's SPU block is not allocated (driver error 0x24)");
    free_pool_block(driver, wave);
}

namespace {

// 80038e6c: set both halves of a volume pair to `value`, then negate one half
// in the negating modes. `reverb` selects the reverb pair's convention.
void set_volume_pair(const SoundDriver &driver, std::array<std::uint16_t, 2> &pair,
                     std::uint32_t value, bool reverb) {
    pair = {static_cast<std::uint16_t>(value), static_cast<std::uint16_t>(value)};
    if ((driver.flags & 0x600U) == 0)
        return;
    // Wide stereo (bit 200) negates the master's right half and the reverb's
    // left half; mode 500 the opposite halves.
    const bool right = ((driver.flags & 0x200U) != 0) != reverb;
    pair[right ? 1 : 0] = static_cast<std::uint16_t>(0U - value);
}

// 80038c68 / 80038d18 with a nonzero frame count: the per-tick step from the
// current 16.16 level toward `volume`. No step is stored when already there.
void fade(std::uint32_t volume, std::uint32_t level, std::uint32_t frames, std::uint32_t &step,
          std::uint16_t &remaining) {
    const auto current = static_cast<std::uint32_t>(static_cast<std::int32_t>(level) >> 8);
    const auto distance = static_cast<std::int32_t>((volume << 8) - current);
    if (distance == 0)
        return;
    const auto divisor = static_cast<std::int32_t>(frames);
    if (distance == std::numeric_limits<std::int32_t>::min() && divisor == -1)
        throw SoundError("A volume fade divides the most negative distance by -1");
    remaining = static_cast<std::uint16_t>(frames);
    step = static_cast<std::uint32_t>(distance / divisor) << 8;
}

// 8003eea0: SPU pitch of a note in 1/256 semitones: the octave/semitone byte
// 80050b78[(note & 7fff) >> 8] selects step row 80050bf0[semitone][note & ff],
// shifted right by 6 - octave (left when negative).
std::int32_t note_pitch(const SoundDriver &driver, std::int32_t note) {
    if (driver.pitch_tables.size() != pitch_table_bytes)
        throw SoundError("The pitch tables are not loaded");
    const auto table = [&](std::uint32_t address) -> std::uint32_t {
        return driver.pitch_tables.at(address - pitch_table_address);
    };
    const auto raw = static_cast<std::uint32_t>(note);
    const auto position = table(pitch_table_address + ((raw & 0x7fffU) >> 8));
    const auto address = 0x80050bf0U + ((raw & 0xffU) + ((position & 0xfU) << 8)) * 2U;
    auto step = static_cast<std::int32_t>(
        static_cast<std::int16_t>(table(address) | (table(address + 1) << 8)));
    const auto shift = 6 - static_cast<std::int32_t>(position >> 4);
    step = shift >= 0 ? step >> shift
                      : static_cast<std::int32_t>(static_cast<std::uint32_t>(step) << -shift);
    return static_cast<std::int16_t>(step);
}

// 8003ef04: claim hardware voice `channel` for a key-on unless a holder has a
// higher priority; a voice the owner holds is marked in voice_holds.
void key_on_voice(SoundDriver &driver, std::uint32_t owner, std::uint32_t channel) {
    if (channel >= driver.voice_owners.size())
        return;
    const auto holder = driver.voice_owners[channel];
    if (holder != owner) {
        if (holder != 0 && s16(u16(driver, owner + 4)) < s16(u16(driver, holder + 4)))
            return;
        put16(driver, owner + 6, 0xffff);
        put16(driver, owner, channel);
        driver.voice_owners[channel] = owner;
        driver.voice_changes |= bit(channel);
    }
    driver.voice_holds |= bit(channel);
}

// 8003efa0: mark hardware voice `channel` for release if `owner` holds it.
void request_release(SoundDriver &driver, std::uint32_t owner, std::uint32_t channel) {
    if (channel < driver.voice_owners.size() && driver.voice_owners[channel] == owner)
        driver.voice_releases |= bit(channel);
}

// 8003ecb0..8003edd0: first and second output of a voice at `volume` and pan
// `pan` (0 .. 4000 centre .. 7f00). Stereo modes (flag 100) use two linear
// ramps that both give 5a00 at the centre; mono gives 5a00 on both sides.
std::array<std::int32_t, 2> pan_law(std::uint32_t flags, std::int32_t volume, std::int32_t pan) {
    if ((flags & 0x100U) == 0) {
        const auto both = (volume * 45 * 512) >> 15;
        return {both, both};
    }
    const auto near = [](std::int32_t p) { return 0x7f00 - ((p * 37 * 256) >> 14); };
    const auto far = [](std::int32_t p) { return (p * 45 * 512) >> 14; };
    const bool first_half = pan < 0x4000;
    const auto distance = first_half ? pan : 0x8000 - pan;
    const auto first = first_half ? near(distance) : far(distance);
    const auto second = first_half ? far(distance) : near(distance);
    return {(first * volume) >> 15, (second * volume) >> 15};
}

} // namespace

void select_sound_mode(SoundDriver &driver, std::int32_t mode) {
    driver.flags &= 0xf8ffU;
    if (mode == static_cast<std::int32_t>(SoundMode::stereo))
        driver.flags |= 0x100U;
    else if (mode == static_cast<std::int32_t>(SoundMode::wide))
        driver.flags |= 0x300U;
    else if (mode == static_cast<std::int32_t>(SoundMode::reverse_wide))
        driver.flags |= 0x500U;
    reapply_volumes(driver);
}

void update_mode_voice(SoundDriver &driver) {
    const auto record = driver.mode_voice;
    if (record == 0 || (u16(driver, record) & 1U) == 0)
        return;
    const auto level = u16(driver, record + 0x12);
    const bool stereo = sound_mode(driver) != 0;
    const auto first = level << (stereo ? 7U : 6U);
    put16(driver, record + 0x38, first);
    put16(driver, record + 0x3a, stereo ? 0U : first);
    put16(driver, record + 0x64, stereo ? 0U : first);
    put16(driver, record + 0x66, first);
    put16(driver, record + 0x36, 1);
    put16(driver, record + 0x62, 1);
}

std::uint32_t sound_mode(const SoundDriver &driver) {
    if ((driver.flags & 0x700U) == 0)
        return 0;
    return (driver.flags & 0x600U) == 0 ? 1 : 2;
}

void set_master_volume(SoundDriver &driver, std::uint32_t volume, std::uint32_t frames) {
    driver.master_target = static_cast<std::uint16_t>(volume);
    if (frames != 0) {
        fade(volume, driver.master_level, frames, driver.master_step, driver.master_frames);
        return;
    }
    driver.master_level = volume << 16;
    driver.master_frames = 0;
    driver.master = static_cast<std::uint16_t>(volume);
    set_volume_pair(driver, driver.master_pair, static_cast<std::uint32_t>(s16(volume)), false);
    driver.commits |= 3U;
}

void set_cd_volume(SoundDriver &driver, std::uint32_t volume, std::uint32_t frames) {
    driver.cd_target = static_cast<std::uint16_t>(volume);
    if (frames != 0) {
        fade(volume, driver.cd_level, frames, driver.cd_step, driver.cd_frames);
        return;
    }
    driver.cd_level = volume << 16;
    driver.cd_frames = 0;
    driver.cd = static_cast<std::uint16_t>(volume);
    driver.cd_pair = {static_cast<std::uint16_t>(volume), static_cast<std::uint16_t>(volume)};
    driver.commits |= 0xc0U;
}

void reapply_volumes(SoundDriver &driver) {
    set_volume_pair(driver, driver.master_pair, static_cast<std::uint32_t>(s16(driver.master)),
                    false);
    driver.cd_pair = {driver.cd, driver.cd};
    set_volume_pair(driver, driver.reverb_pair, static_cast<std::uint32_t>(s16(driver.reverb)),
                    true);
    driver.commits |= 0xc3U;
}

void mark_sequence_voices(SoundDriver &driver, std::uint32_t sequence, std::uint32_t bits) {
    auto count = u8(driver, sequence + 0x14);
    if (count == 0)
        throw SoundError("A sequence without voices loops through the whole counter");
    for (auto record = sequence + voice_records; count != 0; --count, record += voice_stride)
        if (u16(driver, record) != 0)
            put16(driver, record + 2, u16(driver, record + 2) | bits);
}

void mark_sequences(SoundDriver &driver, std::uint32_t bits) {
    for (auto sequence = driver.sequences; sequence != 0; sequence = u32(driver, sequence))
        mark_sequence_voices(driver, sequence, bits);
}

void update_voices(SoundDriver &driver, std::uint32_t sequence, std::uint32_t voices,
                   std::uint32_t count) {
    if ((u16(driver, sequence + 0x10) & 0x20U) != 0)
        return;
    // The loop decrements a 16-bit count after each voice.
    if ((count & 0xffffU) == 0)
        throw SoundError("A zero voice count loops through the whole 16-bit counter");
    for (auto remaining = count & 0xffffU; remaining != 0; --remaining, voices += voice_stride) {
        const auto record = voices;
        if (u16(driver, record) == 0)
            continue;
        const auto changes = u16(driver, record + 2);
        if ((changes & 0x100U) != 0) {
            const auto attenuation = s16(u16(driver, record + 0x7a));
            const auto scale = std::clamp(
                attenuation - ((attenuation * s16(u16(driver, record + 0xd2))) >> 15), 0, 0x7fff);
            auto volume = (s16(u16(driver, record + 0x76)) * scale) >> 15;
            volume = (s16(u16(driver, sequence + 0x72)) * volume) >> 16;
            const auto pan =
                std::clamp(s16(u16(driver, record + 0x74)) + s16(u16(driver, record + 0xd4)) +
                               s16(u16(driver, sequence + 0x8a)),
                           0, 0x7f00);
            const auto [first, second] = pan_law(driver.flags, volume, pan);
            put16(driver, record + 0x38, static_cast<std::uint32_t>(first));
            put16(driver, record + 0x3a, static_cast<std::uint32_t>(second));
            put16(driver, record + 0x36, u16(driver, record + 0x36) | 1U);
        }
        if ((changes & 0x200U) != 0) {
            const auto note = static_cast<std::int16_t>(s16(u16(driver, record + 0x6a)) +
                                                        s16(u16(driver, record + 0xd0)) +
                                                        s16(u16(driver, sequence + 0x7e)));
            put16(driver, record + 0x44,
                  static_cast<std::uint32_t>(note_pitch(driver, note)) & 0x3fffU);
            put16(driver, record + 0x36, u16(driver, record + 0x36) | 4U);
        }
        if ((changes & 1U) != 0 && (u16(driver, record) & 0x20U) == 0)
            key_on_voice(driver, record + 0x30, u8(driver, record + 0x27));
        if ((changes & 2U) != 0)
            request_release(driver, record + 0x30, u8(driver, record + 0x27));
        put16(driver, record + 2, 0);
    }
}

} // namespace xem::reconstruction::resident
