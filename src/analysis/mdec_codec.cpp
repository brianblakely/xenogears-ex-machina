// The analysis host's MDEC bitstream decoder (mdec_codec.hpp). General
// PlayStation MDEC format knowledge: a frame's bitstream starts with six
// halfwords (run-level count, 3800h, quantizer, version, then the first
// data halfword pair); each block's DC coefficient (10 raw bits in version
// 2, a DC-difference code per component in version 3) is followed by
// run-level codes up to an end-of-block code (fe00h), a 7c1fh escape
// taking the next 16 raw bits. Output halfwords are the MDEC's input.
#include "mdec_codec.hpp"

#include <array>
#include <stdexcept>

namespace xem::analysis {
namespace {
constexpr std::uint32_t first_table = 0x801d802c;  // 13-bit lookup, 8-byte entries
constexpr std::uint32_t second_table = 0x801e802c; // 9-bit lookup after an 8-bit prefix
constexpr std::uint32_t escape = 0x7c1f;
constexpr std::uint32_t end_of_block = 0xfe00;

struct Registers {
    std::uint32_t source{}; // next bitstream halfword
    std::uint32_t output{}; // next output halfword
    std::uint32_t bits{};   // the bitstream's next 32 bits, left aligned
    std::uint32_t used{};   // bits of `bits` consumed past its last refill (0-15)
    std::uint32_t quantizer{};
    std::uint32_t block{}; // version 3: component 1-6 (Cr, Cb, then Y); 0: version 2
    std::array<std::uint32_t, 3> dc{}; // version 3 DC predictors: Cr, Cb, Y
};

class Decoder {
  public:
    Decoder(const reconstruction::movie::MdecCodec::Memory &memory) : memory_(memory) {}

    std::uint32_t half(std::uint32_t address) const {
        const auto bytes = memory_(address, 2);
        return static_cast<std::uint32_t>(bytes[0] | bytes[1] << 8U);
    }
    std::uint32_t word(std::uint32_t address) const {
        const auto bytes = memory_(address, 4);
        return static_cast<std::uint32_t>(bytes[0] | bytes[1] << 8U | bytes[2] << 16U |
                                          bytes[3] << 24U);
    }
    void put(std::uint32_t address, std::uint32_t value) const {
        const auto bytes = memory_(address, 2);
        bytes[0] = static_cast<std::uint8_t>(value);
        bytes[1] = static_cast<std::uint8_t>(value >> 8U);
    }
    // Consume `count` bits, then refill 16 once the consumed bits reach 16.
    void consume(Registers &r, std::uint32_t count) const {
        r.bits <<= count & 31U;
        r.used += count;
        refill(r);
    }
    void refill(Registers &r) const {
        const bool due = (r.used & 0x10U) != 0;
        r.used &= 0xfU;
        if (due) {
            r.bits |= half(r.source) << r.used;
            r.source += 2;
        }
    }

  private:
    const reconstruction::movie::MdecCodec::Memory &memory_;
};

std::uint32_t context_word(std::span<const std::uint8_t> context, std::size_t index) {
    const auto at = index * 4;
    return static_cast<std::uint32_t>(context[at] | context[at + 1] << 8U | context[at + 2] << 16U |
                                      context[at + 3] << 24U);
}
void put_context_word(std::span<std::uint8_t> context, std::size_t index, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i)
        context[index * 4 + i] = static_cast<std::uint8_t>(value >> (8U * i));
}
} // namespace

bool LibpressMdecCodec::decode_vlc(const Memory &memory, std::uint32_t bitstream,
                                   std::uint32_t output, std::uint32_t limit,
                                   std::span<std::uint8_t> context) {
    if (context.size() != reconstruction::movie::vlc_context_bytes)
        throw std::invalid_argument("The decode context is 36 bytes");
    const Decoder d(memory);
    Registers r;
    const auto save = [&] {
        const std::array<std::uint32_t, 9> words{r.source,    r.output, r.bits,
                                                 r.used,      r.quantizer, r.block,
                                                 r.dc[0],     r.dc[1],  r.dc[2]};
        for (std::size_t i = 0; i < words.size(); ++i)
            put_context_word(context, i, words[i]);
    };
    enum class Next { dc, ac };
    auto next = Next::dc;
    std::uint32_t end = 0;
    if (bitstream == 0) { // Resume a bounded frame.
        r = {context_word(context, 0), context_word(context, 1), context_word(context, 2),
             context_word(context, 3), context_word(context, 4), context_word(context, 5),
             {context_word(context, 6), context_word(context, 7), context_word(context, 8)}};
        end = r.output + limit * 2U;
        next = Next::ac;
    } else {
        r.output = output;
        end = r.output + limit * 2U;
        const auto count = d.half(bitstream);
        const auto magic = d.half(bitstream + 2);
        r.quantizer = d.half(bitstream + 4) << 10U;
        if (static_cast<std::int32_t>(d.half(bitstream + 6)) - 3 >= 0)
            r.block = 1;
        r.bits = d.half(bitstream + 8) << 16U | d.half(bitstream + 10);
        r.source = bitstream + 12;
        r.used = 0;
        d.put(r.output, count);
        d.put(r.output + 2, magic);
        r.output += 2;
    }
    for (;;) {
        if (next == Next::dc) {
            // A block's DC coefficient; the frame's end marker ends it.
            const auto top = r.bits >> 22U;
            r.output += 2;
            if (r.block == 0) {
                if (top == 0x1ff)
                    break;
                d.consume(r, 10);
                d.put(r.output, r.quantizer | top);
            } else {
                if (top == 0x3ff)
                    break;
                const auto table = first_table - (r.block < 3 ? 1024U : 2048U);
                const auto entry = table + ((r.bits >> 24U) << 2U);
                const auto length = d.half(entry);
                const auto size = d.half(entry + 2);
                std::uint32_t difference = 0;
                r.bits <<= length & 31U;
                if (size != 0) {
                    const auto shift = 32U - size;
                    difference = r.bits >> (shift & 31U);
                    const bool positive = (r.bits & 0x80000000U) != 0;
                    r.bits <<= size & 31U;
                    if (!positive)
                        difference -= 0xffffffffU >> (shift & 31U);
                    r.used += size;
                }
                r.used += length;
                d.refill(r);
                auto &predictor = r.dc[r.block == 1 ? 0 : r.block == 2 ? 1 : 2];
                predictor += difference;
                d.put(r.output, r.quantizer | ((predictor << 2U) & 0x3ffU));
                if (++r.block == 7)
                    r.block = 1;
            }
            const bool bounded = static_cast<std::int32_t>(r.output - end) >= 0;
            r.output += 2;
            if (bounded) {
                save();
                return true;
            }
            next = Next::ac;
        }
        // Run-level codes: a 13-bit lookup yields up to three codes; an
        // empty entry means an 8-bit prefix and a 9-bit lookup.
        auto entry = first_table + ((r.bits >> 19U) << 3U);
        auto first = d.word(entry);
        std::uint32_t more = 0;
        if (first != 0) {
            more = d.word(entry + 4);
        } else {
            d.consume(r, 8);
            entry = second_table + ((r.bits >> 23U) << 2U);
            first = d.word(entry);
        }
        d.consume(r, first & 0xffU);
        // The entry's codes in order: the first always, the second when the
        // entry's second word is nonzero, the third when it is nonzero.
        const std::array<std::uint32_t, 3> codes{first >> 16U, more & 0xffffU, more >> 16U};
        bool block_ended = false;
        bool escaped = false;
        for (std::size_t i = 0; i < codes.size(); ++i) {
            if ((i == 1 && more == 0) || (i == 2 && codes[2] == 0))
                break;
            if (codes[i] == escape) {
                escaped = true;
                break;
            }
            d.put(r.output, codes[i]);
            if (codes[i] == end_of_block) {
                block_ended = true;
                break;
            }
            r.output += 2;
        }
        if (escaped) { // The next 16 raw bits are the code.
            d.put(r.output, r.bits >> 16U);
            r.output += 2;
            r.bits = r.bits << 16U | d.half(r.source) << r.used;
            r.source += 2;
            continue;
        }
        if (block_ended)
            next = Next::dc;
    }
    // The end marker: 65 end-of-block halfwords. (libpress also sets bit 17
    // of the COP0 status register here; nothing in RAM.)
    for (int i = 0; i < 65; ++i) {
        d.put(r.output, end_of_block);
        r.output += 2;
    }
    return false;
}

} // namespace xem::analysis
