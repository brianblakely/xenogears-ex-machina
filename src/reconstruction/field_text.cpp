// Resident 80034f98 and 80034ffc: glyph widths and the glyph image drawn into
// a dialogue line.
#include "xem/reconstruction/field_text.hpp"

#include <bit>

namespace xem::reconstruction::field {

std::uint32_t glyph_width(const TextFont &font, std::uint16_t first, std::uint16_t second) {
    bool narrow = false;
    if (first == 0)
        narrow = std::bit_cast<std::int32_t>(second - font.first_single) < font.narrow_single;
    else if (static_cast<std::int32_t>(first) == font.lead_limit)
        narrow = static_cast<std::int32_t>(second) < font.narrow_double;
    return narrow ? 2 : 3;
}

std::uint32_t glyph_address(const TextFont &font, std::uint16_t first, std::uint16_t second) {
    if (first == 0)
        return font.glyphs + (second - font.first_single) * glyph_bytes;
    if (first == 0xff && second == 0xff)
        return special_glyph;
    return font.glyphs + second * glyph_bytes + font.second_table +
           (first - static_cast<std::uint32_t>(font.lead_limit)) * glyph_bytes * 0x100;
}

GlyphCells glyph_cells(const std::array<std::uint16_t, glyph_rows> &rows, bool odd) {
    // Row bits 80..01 are pixels 1..8, 8000..1000 pixels 9..12.
    constexpr std::array<std::uint16_t, 12> bits{0x80, 0x40, 0x20,   0x10,   0x08,   0x04,
                                                 0x02, 0x01, 0x8000, 0x4000, 0x2000, 0x1000};
    GlyphCells cells{};
    const auto mark = [&](std::size_t row, std::int32_t pixel, std::uint16_t value) {
        if (pixel < 0 || pixel >= 12)
            return;
        const auto p = static_cast<std::uint32_t>(pixel);
        cells[row][p / 4] |=
            static_cast<std::uint16_t>((odd ? value << 2U : value) << (4 * (p % 4)));
    };
    for (std::size_t i = 0; i < glyph_rows; ++i)
        for (std::int32_t bit = 0; bit < 12; ++bit) {
            if ((rows[i] & bits[static_cast<std::size_t>(bit)]) == 0)
                continue;
            const auto pixel = bit + 1;
            for (const auto row : {i, i + 2})
                for (auto x = pixel - 1; x <= pixel + 1; ++x)
                    mark(row, x, 2);
            mark(i + 1, pixel - 1, 2);
            mark(i + 1, pixel, 1);
            mark(i + 1, pixel + 1, 2);
        }
    return cells;
}

} // namespace xem::reconstruction::field
