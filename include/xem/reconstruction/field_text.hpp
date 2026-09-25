#pragma once

#include <array>
#include <cstdint>

namespace xem::reconstruction::field {

// The resident text font parameters (8005934c..80059367). Codes below
// `lead_limit` are one-byte characters starting at `first_single`; a lead byte
// equal to it starts a two-byte character from the second glyph table.
struct TextFont {
    std::int32_t lead_limit{};    // 8005934c
    std::uint32_t second_table{}; // 80059350: byte offset of the two-byte glyphs
    std::int32_t narrow_single{}; // 80059354: narrow one-byte glyphs
    std::int32_t narrow_double{}; // 80059358: narrow two-byte glyphs
    std::uint32_t glyphs{};       // 8005935c: glyph data address
    std::uint32_t first_single{}; // 80059364: first one-byte code
};

inline constexpr std::uint32_t glyph_bytes = 0x16; // eleven rows of 16 bits
inline constexpr std::uint32_t glyph_rows = 11;
// The special glyph 80034ffc draws for the pair (ff, ff), in resident data.
inline constexpr std::uint32_t special_glyph = 0x800501d0;

// Resident 80034f98: a glyph's advance in 4-pixel columns (2 narrow, 3 wide).
[[nodiscard]] std::uint32_t glyph_width(const TextFont &font, std::uint16_t first,
                                        std::uint16_t second);
// The glyph address 80034ffc draws; `first` is zero for a one-byte code.
[[nodiscard]] std::uint32_t glyph_address(const TextFont &font, std::uint16_t first,
                                          std::uint16_t second);

// Resident 80034ffc: a glyph drawn into a 4-bit text image as 13 rows of three
// 16-bit cells (the glyph with a row above and below). Each cell keeps
// `glyph_keep(odd)` of its bits and gains the returned bits: the glyph in one
// two-bit plane (value 1, even line; 4, odd line) with an outline of 2 (or 8)
// around it. Pixel 0 is only ever outline; pixel 12 is clipped.
using GlyphCells = std::array<std::array<std::uint16_t, 3>, glyph_rows + 2>;
[[nodiscard]] GlyphCells glyph_cells(const std::array<std::uint16_t, glyph_rows> &rows, bool odd);
[[nodiscard]] constexpr std::uint16_t glyph_keep(bool odd) { return odd ? 0x3333 : 0xcccc; }

} // namespace xem::reconstruction::field
