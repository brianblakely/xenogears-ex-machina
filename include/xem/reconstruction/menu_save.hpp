#pragma once

#include "xem/reconstruction/menu.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

// Memory-card save and load of the main menu overlay (sha256 82f84a24...,
// loaded at 801c5000) and the resident name codec. Everything here runs on
// menu-mode memory (MenuMemory) at original addresses. Card BIOS I/O is a
// platform service (CardService); the menu's messages, cursor and retries
// stay with their callers.
namespace xem::reconstruction::menu {

class SaveError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// One card block per file: header template byte 3 is 1 (801c6748), and save
// 801cbd90 and load 801cb304 transfer block_count << 13 bytes.
inline constexpr std::size_t file_bytes = 0x2000;
// Header template (card state + 4b94): "SC", icon flag, block count, title,
// palette and icon. Written first, never checked on load.
inline constexpr std::size_t header_bytes = 0x100;
inline constexpr std::size_t payload_bytes = 0x1f00;
// Payload byte holding the eight-bit sum of the bytes before it.
inline constexpr std::size_t checksum_offset = 0x1eff;
inline constexpr std::size_t name_count = 31; // 20-byte names from game data + 0
inline constexpr std::size_t name_bytes = 0x14;
inline constexpr std::size_t gear_count = 20;   // records 11..30 (game data + 978)
inline constexpr std::size_t file_indices = 15; // file name digit 0..14 per card

// Resident words the save and load read or write, owned by menu memory.
// The play counter they also store and restore (80059488) is resident state
// (PadState::vsyncs), passed in.
inline constexpr std::uint32_t saved_globals = 0x8005a3a0;     // 16 halfwords
inline constexpr std::uint32_t text_state = 0x80059360;        // pointer; + 6c code table
inline constexpr std::uint32_t text_single_limit = 0x8005934c; // word
// Menu state + 32c: the card state block (header template at + 4b94).
inline constexpr std::uint32_t state_card = 0x32c;
inline constexpr std::uint32_t card_header = 0x4b94;
// Static overlay byte 801e96a5: nonzero stores disc word 1.
inline constexpr std::uint32_t disc_override = 0x801e96a5;
// Static overlay map 801e9808: pilot character of each gear record.
inline constexpr std::uint32_t gear_pilots = 0x801e9808;

// Resident name codec (80033b34, 80033bac, 80033c20). `table` is the code
// table at *(*80059360 + 6c): two bytes per code, a zero first byte marking
// a one-byte character. Bytes below `single_limit` (*8005934c, signed
// compare) are one-byte characters; others lead a two-byte character.
struct NameCodec {
    std::span<const std::uint8_t> table;
    std::int32_t single_limit{};
};
inline constexpr std::uint16_t missing_code = 0x8000;
inline constexpr std::uint32_t searched_codes = 324;

// The codec as menu memory holds it.
[[nodiscard]] NameCodec name_codec(const MenuMemory &memory);

// 80033bac: the first code among 324 whose table pair is (first, second), or
// 8000.
[[nodiscard]] std::uint16_t find_name_code(const NameCodec &codec, std::uint8_t first,
                                           std::uint8_t second);

// 80033c20 over `text` (its caller's buffer, as far as it is known). Codes
// are appended until a zero byte where a character starts; a lead byte takes
// the next byte, even a zero one, and reading continues after it. An
// unknown pair stores 8000 and ends with result -1; otherwise the result is
// 0. Reading past `text` is unrecovered caller memory and throws.
struct EncodedText {
    std::vector<std::uint16_t> codes;
    std::int32_t result{};
};
[[nodiscard]] EncodedText encode_text(const NameCodec &codec, std::span<const std::uint8_t> text);

// 80033b34: `count` little-endian codes from `codes` become their one or two
// bytes each, then a zero byte. Returns the bytes written.
[[nodiscard]] std::vector<std::uint8_t>
decode_text(const NameCodec &codec, std::span<const std::uint8_t> codes, std::uint32_t count);

// 801cb184's twenty-byte output buffer. It is never cleared: each decoded
// name overwrites a prefix, and all twenty bytes return to the game data, so
// bytes after a name's terminator come from the previous name or, for the
// first, from whatever the caller's stack held. Callers pass it in and get
// its final contents back.
using NameScratch = std::array<std::uint8_t, name_bytes>;

// 801cb184: decode the 31 stored names of the game data in place.
void decode_names(Menu &menu, NameScratch &scratch);

// 801e4a28: copy the game data into the payload at `payload`.
void store_game_data(Menu &menu, std::uint32_t payload);

// 801cba4c(payload, card, digit) on a zeroed payload: party summary, play
// counter and file digit, then in the game data itself the globals (game +
// 2324), the names in their encoded form and the disc word (game + 19d4),
// the copy of 801e4a28, and 801cb184 decoding the names back. `disc` is
// the resident 80028530 result (u16 at the loaded directory table + 78).
void serialize(Menu &menu, std::uint32_t payload, std::uint32_t digit, std::uint32_t disc,
               std::uint32_t play_frames, NameScratch &scratch);

// 801cc424..801cc448: store the eight-bit sum of payload[0..1eff) at
// payload + 1eff. Returns the full sum the loop accumulates (A1 at 801cc44c).
std::uint32_t seal_payload(Menu &menu, std::uint32_t payload);

// Load decision 801cb6f0..801cb71c over the file read to `buffer` (header,
// then payload): the only validation the load makes.
enum class LoadDecision {
    accepted,          // 801cb5b4: 801cb28c(buffer + 100) restores it
    checksum_mismatch, // 801cb724: buffer released, message 3e, card state + 4fe6 = 2
};
struct LoadCheck {
    std::uint8_t sum{};    // V0 at 801cb71c
    std::uint8_t stored{}; // buffer + 1fff
    LoadDecision decision{};
};
[[nodiscard]] LoadCheck check_loaded(const Menu &menu, std::uint32_t buffer);
inline constexpr std::uint32_t checksum_mismatch_message = 0x3e;

// 801e4d10(payload, tables): restore the game data from a payload; gear
// records keep their unstored bytes and recompute derived values from the
// data tables of directory `tables` (801e41c0, 801e4258, 801e42ac,
// 801e433c with jump table 801c524c, level 801e4928).
void restore_game_data(Menu &menu, std::uint32_t payload, std::uint32_t tables);

// 801cb28c(payload): 801e4d10 with the menu's table directory, the play
// counter from payload + 0, the globals from game + 2324, then 801cb184.
void apply_loaded(Menu &menu, std::uint32_t payload, std::uint32_t &play_frames,
                  NameScratch &scratch);

// Load slot selection in 801cb304 (801cb3b8..801cb3e4). Slot s (0..29) is
// card entry m = *(801e981c + 4s) (port m / 16). 801c9bcc(1) keeps the
// cursor slot (card state + 4f7c) when it is loadable: files listed (card
// state + 4f88, upper halfword nonzero), port present (+4fe4 + port), an
// entry there (+4fae + m not ff) and its +4f8e + m byte nonzero (set by the
// directory scan; read here as "this game's file", inferred).
// 801c9d34(1) otherwise returns the first loadable slot of the present
// ports, stopping at the first slot when no files are listed, or ff. Both
// set menu state + 4d8 to 2 (801c9bcc only when loadable). ff is "No data.":
// message 62 and the load returns (observed on the blank-card route). Only
// mode 1, the load's, is recovered; other modes throw MissingDependency.
[[nodiscard]] bool load_slot_valid(Menu &menu, std::uint32_t mode);
[[nodiscard]] std::uint32_t find_load_slot(Menu &menu, std::uint32_t mode);
inline constexpr std::uint32_t no_data_message = 0x62;

// Card decision of 801c93a8 at 801c9400: the per-port card presence bytes
// at card state + 4fe4/4fe5 (801c8a10 clears a port's byte when its card
// answers -1). Both clear: message 23 and the load returns.
enum class CardPresence { present, no_card };
[[nodiscard]] CardPresence card_presence(const Menu &menu);
inline constexpr std::uint32_t no_card_message = 0x23;

// Save path file name: "bu00:" or "bu10:" + "BASLUS-00664" + ('0' + digit).
[[nodiscard]] std::string file_name(unsigned card, unsigned digit);

// 801cb9e8: an overwritten file keeps its digit; otherwise the first index
// whose used flag (801ea6d0 + card * 10) is clear, or 0 when all are used.
[[nodiscard]] std::uint8_t choose_file_digit(std::span<const std::uint8_t, 16> used,
                                             std::uint8_t existing);

// The card file 801cbd90 writes: the header template (card state + 4b94,
// 100 bytes) in one write, then the sealed payload in 100-byte chunks.
[[nodiscard]] std::vector<std::uint8_t> save_file_block(const Menu &menu, std::uint32_t payload);

} // namespace xem::reconstruction::menu
