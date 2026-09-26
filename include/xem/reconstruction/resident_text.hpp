#pragma once

#include "xem/reconstruction/resident_memory.hpp"

#include <cstdint>
#include <functional>

// Resident text, sprite-sheet and resource helpers of executable dc0b2dd7...
// that several modes call (the menu overlay, battle, field). Each works on
// memory at original addresses through resident::Memory; arguments are the
// argument registers and caller stack words each routine reads, in order, and
// the result is V0. Other resident dependencies are explicit parameters.
namespace xem::reconstruction::resident {

// ---- Offset tables ----------------------------------------------------------
// A table of halfword offsets from +4 (entry i at +4 + 2i), each relative to
// the table itself.

// 80033728(table, index): the address of entry `index`.
[[nodiscard]] std::uint32_t offset_table_entry(const Memory &memory, std::uint32_t table,
                                               std::uint32_t index);
// Name tables: offset tables whose addresses are text state (*80059360)
// words, indexed by an id byte. The menu reads inventory item names through
// 80033818 (the item list, 801da5bc) and equipment names through the others:
// the equipment panels (801d9090.., 801ded8c..) take 80033848 / 800337e8 for
// a character's weapon and accessory slots and 80033a5c / 80033a2c for the
// same slots of a gear.
// 800337e8(id): accessory name (text state +44).
[[nodiscard]] std::uint32_t accessory_name(const Memory &memory, std::uint32_t id);
// 80033818(id): item name (text state +58).
[[nodiscard]] std::uint32_t item_name(const Memory &memory, std::uint32_t id);
// 80033848(id): weapon name (text state +5c).
[[nodiscard]] std::uint32_t weapon_name(const Memory &memory, std::uint32_t id);
// 80033a2c(id): gear accessory name (text state +c8).
[[nodiscard]] std::uint32_t gear_accessory_name(const Memory &memory, std::uint32_t id);
// 80033a5c(id): gear weapon name (text state +cc).
[[nodiscard]] std::uint32_t gear_weapon_name(const Memory &memory, std::uint32_t id);

// 8003342c(list): a word count followed by that many offsets relative to the
// list; each offset becomes an address. Returns the count.
std::uint32_t relocate_offsets(Memory &memory, std::uint32_t list);

// ---- Packed resources -------------------------------------------------------
// 80031bdc(size, mode) from the JAL at 80032e94: a new block's address.
using Allocate = std::function<std::uint32_t(std::uint32_t size, std::uint32_t mode)>;
inline constexpr std::uint32_t unpack_allocation_site = 0x80032e94;
// 80032e88(packed, mode): a new block (80031bdc(size, mode), the size being
// the packed data's first word) holding the data unpacked (80032eb4, the
// field decoder). Returns the block, zero when the allocation returns zero.
std::uint32_t unpack_to_new_block(Memory &memory, std::uint32_t packed, std::uint32_t mode,
                                  const Allocate &allocate);

// 8002dd20(list): load each TIM of a list (a word count, then that many
// offsets relative to the list, low two bits ignored) into VRAM, last first:
// OpenTIM (800471b4) and ReadTIM (800471c4) parse it, then DrawSync(0)
// and LoadImage of the CLUT when present, DrawSync(0) and LoadImage of the
// pixels. `load_image(rect, data)` is LoadImage (80044894), `draw_sync`
// DrawSync(0) (800445d0).
void load_tim_list(Memory &memory, std::uint32_t list, const std::function<void()> &draw_sync,
                   const std::function<void(std::uint32_t rect, std::uint32_t data)> &load_image);

// ---- Sprite sheets ----------------------------------------------------------
// A sheet is an offset table of sprites. A sprite is a halfword part count
// (+0) then, from +4, 1c-byte parts: texture u, v, w, h (+0..+6), offset
// x, y (+8, +a), texture page depth (+10), CLUT x, y (+12, +14), texture
// page x, y (+16, +18), mirror x and y bytes (+1a, +1b). Each part draws as
// one POLY_FT4 per draw buffer (0x28 bytes each, 0x50 per part).

// 80026338(sheet, id, *count, *depth, *clut_x, *clut_y, *vram_x, *vram_y):
// sprite `id`'s part count and its first part's texture: depth, CLUT
// position and the VRAM position of its texels: the page x (low 6 bits
// cleared) plus u (signed) >> 4 at depth 0, >> 2 otherwise; the page y (low
// 8 bits cleared) plus v.
void sheet_part_texture(Memory &memory, std::uint32_t sheet, std::uint32_t id,
                        std::uint32_t count_out, std::uint32_t depth_out, std::uint32_t clut_x_out,
                        std::uint32_t clut_y_out, std::uint32_t vram_x_out,
                        std::uint32_t vram_y_out);
// 8002675c(sheet, id, packets, buffer, x, y, scale): sprite `id` as POLY_FT4
// packets from `packets` (+ buffer * 28 in each part's 50 bytes) at x, y,
// offsets and sizes scaled by scale / 1000h. Returns the part count.
std::uint32_t sheet_quads(Memory &memory, std::uint32_t sheet, std::uint32_t id,
                          std::uint32_t packets, std::uint32_t buffer, std::uint32_t x,
                          std::uint32_t y, std::uint32_t scale);
// 800263e4(sheet, id, packets, buffer, x, y, scale, flip_x, flip_y): as
// sheet_quads, the sprite also mirrored about x, y when flip_x / flip_y
// (bytes) are set; texture edges follow the drawn orientation.
std::uint32_t sheet_quads_flipped(Memory &memory, std::uint32_t sheet, std::uint32_t id,
                                  std::uint32_t packets, std::uint32_t buffer, std::uint32_t x,
                                  std::uint32_t y, std::uint32_t scale, std::uint32_t flip_x,
                                  std::uint32_t flip_y);

// ---- Text -------------------------------------------------------------------
// The resident text-layout window at 80059fd8 (the dialogue window record
// layout: cursor +0, line +2, width +8/+a, lines +c, flags +10, stride +12,
// text +1c, image +2c, line records +28 -> 8005a068, glyph budget +69) that
// 80034eac lays one line of text into.
inline constexpr std::uint32_t layout_window = 0x80059fd8;
inline constexpr std::uint32_t layout_line = 0x8005a068;
// 80034eac(text, image, width, plane): lay `text` out as one line of glyphs
// into the image at `image` (glyph cursor limit width | 1, row stride
// (width | 1) + 3 halfwords), in the bit plane `plane` (bit 0) selects,
// through the glyph renderer 80033df0 (`glyphs(window)`, run on
// layout_window). Returns the line's width in pixels (its glyph width * 4).
std::uint32_t layout_text_line(Memory &memory, std::uint32_t text, std::uint32_t image,
                               std::uint32_t width, std::uint32_t plane,
                               const std::function<void(std::uint32_t window)> &glyphs);

} // namespace xem::reconstruction::resident
