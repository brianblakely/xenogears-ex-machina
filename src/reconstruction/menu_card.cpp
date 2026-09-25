// Menu overlay 82f84a24... (801c5000): memory-card management around the
// save and load decisions. Card status polling (801c8bec), the directory
// scan and file heads (801c8d78, 801c90b0), this game's files (801c9270),
// the card refresh (801c93a8), the slot cursor, the yes/no confirmation,
// the file screen's details panel, and the save (801cbd90) and load
// (801cb304) themselves. BIOS card calls are the CardBios service; the save
// payload, its checksum and the game-data copies are menu_save.hpp.
#include "xem/reconstruction/menu_overlay.hpp"

#include "xem/reconstruction/menu_save.hpp"
#include "xem/reconstruction/resident_text.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace xem::reconstruction::menu {
namespace {

// ---- Menu state fields (offsets from *800625a0) -----------------------------
constexpr std::uint32_t sprite_sheet = 0x2dc;    // sprite sheet (8002675c)
constexpr std::uint32_t label_text = 0x2e0;      // label text offset table
constexpr std::uint32_t effect_bank = 0x2e4;     // effect bank object (id at +14)
constexpr std::uint32_t buffer_index = 0x308;    // draw buffer being built (0/1)
constexpr std::uint32_t number_digits = 0x322;   // three digit sprites (801c80b8), ff blank
constexpr std::uint32_t input_code = 0x325;      // the frame's decoded input
constexpr std::uint32_t card_poll_timer = 0x326; // frames since the last card poll
constexpr std::uint32_t view_motion = 0x329;     // nonzero while the view moves
constexpr std::uint32_t sounds = 0x32a;          // nonzero plays menu effects
constexpr std::uint32_t b_334 = 0x334;           // file screen request (1 after card work)
constexpr std::uint32_t top_cursor = 0x336;       // top command cursor
constexpr std::uint32_t file_choice = 0x338;     // file command: 0 delete, 1 copy, 2 save/load
constexpr std::uint32_t file_details = 0x34c;     // file details block (sprites, packets)
constexpr std::uint32_t message_window = 0x388;  // message window record (+11 open)
constexpr std::uint32_t file_blocks = 0x3a8;     // 32 blocks the file screen releases
constexpr std::uint32_t markers = 0x428;         // cursor and yes/no marker block
constexpr std::uint32_t access_sprite = 0x44c;   // card access indicator block (7bc bytes)
constexpr std::uint32_t load_state = 0x4d8;
constexpr std::uint32_t message_lines = 0x1de0; // four 80-byte message line records

// Party block (*(state + 33c)) bytes.
constexpr std::uint32_t party_07 = 0x07;
constexpr std::uint32_t party_08 = 0x08;
constexpr std::uint32_t file_details_shown = 0x0b;
constexpr std::uint32_t message_shown = 0x22;
constexpr std::uint32_t message_open = 0x2e;
constexpr std::uint32_t cursor_active = 0x2f;
constexpr std::uint32_t message_pending = 0x33;
constexpr std::uint32_t party_4b = 0x4b;
constexpr std::uint32_t access_state = 0x52; // 0 none, 1 shown, 2 closing

// Marker block (*(state + 428)): +140, yes/no highlights +142/+143, cursor
// shown +144, the cursor's marker record +148 (28h-byte records, corner
// x, y at +8/+a, +10/+12, +18/+1a, +20/+22).
constexpr std::uint32_t markers_140 = 0x140;
constexpr std::uint32_t yes_marker = 0x142;
constexpr std::uint32_t no_marker = 0x143;
constexpr std::uint32_t cursor_shown = 0x144;
constexpr std::uint32_t cursor_record = 0x148;

// ---- Card state block (*(state + 32c)) --------------------------------------
// File entries: 32 of 5ch bytes (index port * 16 + n): icon frame rows +0..
// +14, name +18, icon shown +58.
constexpr std::uint32_t entry_bytes = 0x5c;
constexpr std::uint32_t entry_name = 0x18;
constexpr std::uint32_t entry_icon_shown = 0x58;
// File heads: 32 of 200h bytes from +b94, the first 200h bytes of each file
// (header 100h: +2 icon flag, +3 block count, +4 title, +60 palette, +80
// icon frames; then the payload's first 100h bytes).
constexpr std::uint32_t heads = 0xb94;
constexpr std::uint32_t head_bytes = 0x200;
constexpr std::uint32_t port_status = 0x4f74;      // word per port: 0, -1, -2, -3
constexpr std::uint32_t cursor_slot = 0x4f7c;      // word: slot 0..29
constexpr std::uint32_t shown_slot = 0x4f80;       // word: slot the details show
constexpr std::uint32_t listed_count = 0x4f84;     // word: position entries listed
constexpr std::uint32_t port_scanned = 0x4f88;     // byte per port
constexpr std::uint32_t port_files = 0x4f8a;       // byte per port: files found
constexpr std::uint32_t port_files_seen = 0x4f8c;  // byte per port: count listed, ff none
constexpr std::uint32_t game_file = 0x4f8e;        // byte per position: this game's file
constexpr std::uint32_t position_entry = 0x4fae;   // byte per position: entry, ff none
constexpr std::uint32_t game_prefix = 0x4fce;      // this game's file name prefix
constexpr std::uint32_t port_present = 0x4fe4;     // byte per port
constexpr std::uint32_t poll_mode = 0x4fe6;        // 0 off, 1 scan and flag, 2 scan
constexpr std::uint32_t b_4fe7 = 0x4fe7;
constexpr std::uint32_t presence_seen = 0x4fe8;    // byte per port
constexpr std::uint32_t io_event = 0x4fec;         // card events: done,
constexpr std::uint32_t error_event = 0x4ff0;      // error,
constexpr std::uint32_t timeout_event = 0x4ff4;    // timeout,
constexpr std::uint32_t new_card_event = 0x4ff8;   // new card
constexpr std::uint32_t title_suffix = 0x4ffc;     // text appended to the save title

// ---- Overlay static data ----------------------------------------------------
constexpr std::uint32_t device_names = 0x801c50a8; // two 8-byte device names (6 copied)
constexpr std::uint32_t temp_suffix = 0x801c50b8;  // temporary file name suffix
constexpr std::uint32_t title_template = 0x801c50e0; // 1bh bytes
constexpr std::uint32_t status_codes = 0x801e9768;   // event index -> status word
constexpr std::uint32_t card_changed = 0x801e9778;
constexpr std::uint32_t poll_period = 0x801e9779;
constexpr std::uint32_t watch_cards = 0x801e977a;
constexpr std::uint32_t slot_positions = 0x801e981c; // 30 words: slot -> position
constexpr std::uint32_t position_x = 0x801e9894;     // halfword per position (stride 4)
constexpr std::uint32_t position_y = 0x801e9914;
constexpr std::uint32_t label_x = 0x801e9f98;        // 9 words
constexpr std::uint32_t label_y = 0x801e9fbc;        // 9 words
constexpr std::uint32_t time_x = 0x801e9fe0;         // colon x twice, then 7 digit x
constexpr std::uint32_t member_x = 0x801ea004;       // 3 words
constexpr std::uint32_t member_y = 0x801ea010;       // 3 words
constexpr std::uint32_t level_x = 0x801ea01c, level_y = 0x801ea020;
constexpr std::uint32_t hp_x = 0x801ea02c, hp_y = 0x801ea030;
constexpr std::uint32_t max_hp_x = 0x801ea034, max_hp_y = 0x801ea038;
constexpr std::uint32_t stat_x = 0x801ea03c, stat_y = 0x801ea040;
constexpr std::uint32_t stat2_x = 0x801ea044, stat2_y = 0x801ea048;
constexpr std::uint32_t title_x = 0x801ea04c, title_y = 0x801ea050; // halfwords
constexpr std::uint32_t label_ids = 0x801ea494;      // 9 words, ffff none
constexpr std::uint32_t name_image_x = 0x801ea590;   // per column (stride 4)
constexpr std::uint32_t name_image_y = 0x801ea5dc;
constexpr std::uint32_t ascii_codes = 0x801ea5d0;    // Shift-JIS code per ASCII byte
constexpr std::uint32_t used_digits = 0x801ea6d0;    // 16 bytes per port
constexpr std::uint32_t last_payload = 0x801ea6f4;
constexpr std::uint32_t cards_reset = 0x801ea6f8;
constexpr std::uint32_t saved_callbacks = 0x801ea718; // three words
constexpr std::uint32_t two_byte_character = 0x801ea8c0;
constexpr std::uint32_t icon_palette = 0x801ea8c4;   // 20h bytes
constexpr std::uint32_t icon_rect = 0x801ea8e4;      // RECT
constexpr std::uint32_t palette_rect = 0x801ea8ec;   // RECT
constexpr std::uint32_t card_cancelled = 0x801ea8fc;
constexpr std::uint32_t port_blocks = 0x801ea900;    // word per port: blocks listed

// ---- Game data --------------------------------------------------------------
constexpr std::uint32_t game = 0x8006d634;
constexpr std::uint32_t game_globals = game + 0x2324; // 20h bytes (8006f958)
constexpr std::uint32_t game_disc = game + 0x19d4;    // u16 (8006f008)
constexpr std::uint32_t gear_records = 0x8006dfac;    // records 11..30
constexpr std::uint32_t record_bytes = 0xa4;
constexpr std::uint32_t menu_mode = 0x80059460;       // 2: title file screen (load)
constexpr std::uint32_t cd_read_callback = 0x80056844; // 8004373c's word

// The card event class and the specs of the four events (general BIOS:
// SwCARD f4000001; spec 4 done, 8000 error, 100 timeout, 2000 new card).
constexpr std::uint32_t card_class = 0xf4000001;
constexpr std::array<std::uint32_t, 4> card_specs{4, 0x8000, 0x100, 0x2000};
constexpr std::uint32_t none = 0xffffffffU; // -1
// menu_save.hpp sizes as words.
constexpr auto names = static_cast<std::uint32_t>(name_count);
constexpr auto name_size = static_cast<std::uint32_t>(name_bytes);
constexpr auto payload_size = static_cast<std::uint32_t>(payload_bytes);
constexpr auto header_size = static_cast<std::uint32_t>(header_bytes);

std::uint32_t signed_div(std::uint32_t value, std::int32_t by) {
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value) / by);
}

CardBios &bios(const Overlay &overlay, const char *call) {
    if (overlay.card == nullptr)
        throw ServiceUnavailable(std::string("memory card BIOS ") + call);
    return *overlay.card;
}

std::string text_at(const Overlay &overlay, std::uint32_t address) {
    std::string text;
    for (auto byte = overlay.u8(address); byte != 0; byte = overlay.u8(++address))
        text.push_back(static_cast<char>(byte));
    return text;
}

std::vector<std::uint8_t> bytes_at(const Overlay &overlay, std::uint32_t address,
                                   std::uint32_t count) {
    std::vector<std::uint8_t> bytes(count);
    for (std::uint32_t i = 0; i < count; ++i)
        bytes[i] = static_cast<std::uint8_t>(overlay.u8(address + i));
    return bytes;
}

// firstfile / nextfile: the entry is stored at `dir`, V0 is `dir` or 0.
std::uint32_t store_entry(Overlay &overlay, std::uint32_t dir,
                          const std::optional<CardBios::DirectoryEntry> &entry) {
    if (!entry)
        return 0;
    for (std::uint32_t i = 0; i < entry->size(); ++i)
        overlay.put8(dir + i, (*entry)[i]);
    return dir;
}

std::uint32_t card_open(Overlay &overlay, std::uint32_t name, std::uint32_t mode) {
    return bios(overlay, "open").open(text_at(overlay, name), mode); // 80040534
}

std::uint32_t card_read(Overlay &overlay, std::uint32_t fd, std::uint32_t buffer,
                        std::uint32_t count) { // 80040544
    const auto read = bios(overlay, "read").read(fd, count);
    if (read.bytes.size() > count)
        throw ServiceUnavailable("memory card BIOS read returned more than the count");
    for (std::uint32_t i = 0; i < read.bytes.size(); ++i)
        overlay.put8(buffer + i, read.bytes[i]);
    return read.result;
}

std::uint32_t card_write(Overlay &overlay, std::uint32_t fd, std::uint32_t buffer,
                         std::uint32_t count) { // 80040554
    const auto bytes = bytes_at(overlay, buffer, count);
    return bios(overlay, "write").write(fd, bytes);
}

std::uint32_t card_close(Overlay &overlay, std::uint32_t fd) {
    return bios(overlay, "close").close(fd); // 80040564
}

std::uint32_t card_erase(Overlay &overlay, std::uint32_t name) {
    return bios(overlay, "erase").erase(text_at(overlay, name)); // 800405b4
}

// A failed card call before a retry: 801c8ca4 (outside the census) waits a
// second with the poll mode at 2 unless the port is unformatted.
[[noreturn]] void retry_wait(std::uint32_t site) {
    Overlay::missing("menu_card_retry", site, "symbol:menu-card-retry-wait-801c8ca4",
                     "A failed card call waits in 801c8ca4, outside the census, before it "
                     "retries");
}

// The device name of `port` ("bu00:" / "bu10:" in the image): its first
// word and halfword, as the original copies them.
void copy_device(Overlay &overlay, std::uint32_t to, std::uint32_t port) {
    const auto from = device_names + (port != 0 ? 8U : 0U);
    overlay.put32(to, overlay.u32(from));
    overlay.put16(to + 4, overlay.u16(from + 4));
}

// The name codec (80033b34 / 80033c20) over the loaded code table.
NameCodec codec(const Overlay &overlay) {
    const auto table = overlay.u32(overlay.u32(text_state) + 0x6c);
    return {overlay.program.menu->tail(table),
            static_cast<std::int32_t>(overlay.u32(text_single_limit))};
}

// 80033b34(codes, text, count): `count` codes become their characters and a
// zero byte at `text`.
void decode_codes(Overlay &overlay, std::uint32_t codes, std::uint32_t text, std::uint32_t count) {
    const auto source = bytes_at(overlay, codes, count * 2);
    const auto decoded = decode_text(codec(overlay), source, count);
    for (std::uint32_t i = 0; i < decoded.size(); ++i)
        overlay.put8(text + i, decoded[i]);
}

// 80033b34's call shape in 801cb184 and 801e71b4: the name's code pairs up
// to the first zero pair (which is copied too) into `codes`; returns the
// code count.
std::uint32_t copy_name_codes(Overlay &overlay, std::uint32_t name, std::uint32_t codes) {
    std::uint32_t at = 0;
    for (; at < name_size; at += 2) {
        overlay.put8(codes + at, overlay.u8(name + at));
        overlay.put8(codes + at + 1, overlay.u8(name + at + 1));
        if (overlay.u8(name + at) == 0 && overlay.u8(name + at + 1) == 0)
            break;
    }
    return at / 2;
}

// 80039e60(code): start effect `code` when effects are enabled; the voice
// search it starts with (8003a65c) is not recovered.
void menu_effect(Overlay &overlay, std::uint32_t code, std::uint32_t site) {
    static_cast<void>(code);
    if ((overlay.program.resident.sound.flags & 0x800U) == 0)
        return;
    Overlay::missing("menu_effect", site, "symbol:sound-effect-voice-8003a65c",
                     "80039e60 starts an effect on voices 8003a65c searches; that search is "
                     "not recovered");
}

} // namespace

// ---- Card events and status ---------------------------------------------------

// 801c87c4: UnDeliverEvent (800404c4) the four card events.
void Overlay::undeliver_card_events() {
    const auto stack_frame = enter(0x18);
    for (const auto spec : card_specs)
        bios(*this, "UnDeliverEvent").undeliver_event(card_class, spec);
}

// 801c881c: wait for one of the card events (TestEvent 80040494, polled in
// the order new card, error, done, timeout), undeliver all four (801c87c4)
// and return its index: 3 new card, 1 error, 0 done, 2 timeout.
std::uint32_t Overlay::wait_card_event() {
    const auto stack_frame = enter(0x18);
    const auto cards = u32(at(state_card));
    auto &service = bios(*this, "TestEvent");
    for (;;) {
        if (service.test_event(u32(cards + new_card_event)) == 1) {
            undeliver_card_events();
            return 3;
        }
        if (service.test_event(u32(cards + error_event)) == 1) {
            undeliver_card_events();
            return 1;
        }
        if (service.test_event(u32(cards + io_event)) == 1) {
            undeliver_card_events();
            return 0;
        }
        if (service.test_event(u32(cards + timeout_event)) == 1) {
            undeliver_card_events();
            return 2;
        }
    }
}

// 801c891c(port): the status of card `port` (0 or 10h, passed through to
// _card_info 8004e784): -1 when the request is refused, else the status word
// 801e9768 holds for the delivered event (0 ready, -3 error, -1 timeout, -2
// new card).
std::uint32_t Overlay::card_status(std::uint32_t a0) {
    const auto stack_frame = enter(0x18);
    if (bios(*this, "_card_info").card_info(a0) == 0)
        return none;
    return u32(status_codes + (wait_card_event() & 0xffU) * 4);
}

// 801c8960: one frame (801c7bf4), then close the four card events
// (CloseEvent 80040484) inside a critical section.
void Overlay::close_card_events() {
    const auto stack_frame = enter(0x18);
    menu_frame();
    auto &service = bios(*this, "CloseEvent");
    static_cast<void>(service.enter_critical_section()); // 800404d4
    const auto cards = u32(at(state_card));
    for (const auto event : {io_event, error_event, timeout_event, new_card_event})
        static_cast<void>(service.close_event(u32(cards + event)));
    service.exit_critical_section(); // 800404e4
}

// 801c8a10(port): poll card `port`: mark it present and read its status
// (801c891c) into the port's status word; a ready answer (0) from a port
// whose word was -1 counts as 1 and leaves the word 0. A card that is gone
// (-1) clears the port: no files, not present, no blocks listed, every
// position empty and no icon. Returns 0 for an unformatted card (-2), else 1.
// A presence change clears the port's scanned flag.
std::uint32_t Overlay::poll_card_port(std::uint32_t a0) {
    const auto stack_frame = enter(0x20);
    const auto port = a0 & 0xffU;
    const auto cards = u32(at(state_card));
    std::uint32_t result = 1;
    put8(cards + port + port_present, 1);
    auto status = card_status(port != 0 ? 0x10U : 0U);
    const auto status_word = cards + port * 4 + port_status;
    if (status == 0 && u32(status_word) == none) {
        status = 1;
        put32(status_word, 0);
    } else {
        put32(status_word, status);
    }
    if (status == none) {
        put8(cards + port + port_files, 0);
        put8(cards + port + port_present, 0);
        put32(port_blocks + port * 4, 0);
        for (std::uint32_t n = 0; n < 16; ++n) {
            const auto position = port * 16 + n;
            put8(cards + position + position_entry, 0xff);
            put8(cards + position + game_file, 0);
            put8(cards + position * entry_bytes + entry_icon_shown, 0);
        }
    } else if (status == 0xfffffffeU) {
        result = 0;
    }
    if (u8(cards + port + presence_seen) != u8(cards + port + port_present)) {
        put8(cards + port + port_scanned, 0);
        put8(cards + port + presence_seen, u8(cards + port + port_present));
    }
    return result;
}

// 801c8bec: while card polling is on (card state + 4fe6), count frames and
// every period (801e9779) poll both ports (801c8a10); with both timed out
// the file screen request (state + 334) clears.
void Overlay::poll_cards() {
    const auto stack_frame = enter(0x18);
    const auto cards = u32(at(state_card));
    if (u8(cards + poll_mode) == 0)
        return;
    const auto frames = (u8(at(card_poll_timer)) + 1) & 0xffU;
    put8(at(card_poll_timer), frames);
    if (u8(poll_period) >= frames)
        return;
    static_cast<void>(poll_card_port(0));
    static_cast<void>(poll_card_port(1));
    if (u32(cards + port_status) == none && u32(cards + port_status + 4) == none)
        put8(at(b_334), 0);
    put8(at(card_poll_timer), 0);
}

// 801c8d78(port): list the port's directory: clear the port's blocks listed,
// its position entries (ff) and used digits, then copy each directory
// entry's name (firstfile 80040584 / nextfile 80040594 on the device name)
// into the file entries from port * 16. Stores and returns the count.
std::uint32_t Overlay::list_card_directory(std::uint32_t a0) {
    const auto stack_frame = enter(0x50);
    auto f = frame(0x50);
    const auto dir = f[0x10];
    const auto pattern = f[0x38];
    const auto port = a0 & 0xffU;
    put32(port_blocks + port * 4, 0);
    for (std::uint32_t n = 0; n < 16; ++n) {
        put8(u32(at(state_card)) + port * 16 + n + position_entry, 0xff);
        put8(used_digits + port * 16 + n, 0);
    }
    copy_device(*this, pattern, port);
    // The retry counter (5, decremented once) never reaches zero here.
    std::uint32_t count = 0;
    auto &service = bios(*this, "firstfile");
    if (store_entry(*this, dir, service.first_file(text_at(*this, pattern))) == dir) {
        do {
            const auto entry = port * 16 + (count & 0xffU);
            ++count;
            strcpy(u32(at(state_card)) + entry * entry_bytes + entry_name, dir);
        } while (store_entry(*this, dir, service.next_file()) == dir);
    }
    put8(u32(at(state_card)) + port + port_files, count);
    return count & 0xffU;
}

// 801c8ee8: after each frame, list the ports not yet listed (801c8d78). In
// poll mode 1 a listing that finds files raises the file screen request
// (state + 334); mode 2 only lists.
void Overlay::list_unscanned_ports() {
    const auto stack_frame = enter(0x18);
    const auto cards = u32(at(state_card));
    const auto mode = u8(cards + poll_mode);
    if (mode != 1 && mode != 2)
        return;
    for (std::uint32_t port = 0; port < 2; ++port) {
        if (u8(cards + port_scanned + port) != 0)
            continue;
        const auto found = list_card_directory(port) & 0xffU;
        if (mode == 1 && found != 0)
            put8(at(b_334), 1);
        put8(cards + port_scanned + port, 1);
    }
}

// 801c9038(name, buffer): read the first 200h bytes of file `name` (open
// mode 3, read, close). Returns 0, or -1 when the open or the read fails.
std::uint32_t Overlay::read_file_head(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x20);
    const auto fd = card_open(*this, a0, 3);
    if (fd == none)
        return none;
    const auto read = card_read(*this, fd, a1, 0x200);
    static_cast<void>(card_close(*this, fd));
    return read == 0x200 ? 0U : none;
}

// 801c90b0(port, n): read file entry port * 16 + n's head (801c9038 on the
// device name + entry name) into its head buffer and list it: one position
// entry per block of the file (head byte 3), each naming the entry, from the
// running count (card state + 4f84). The read's result is not checked (its
// retry counter starts at 1, so the retry wait 801c8ca4 is never reached).
void Overlay::load_file_head(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x78);
    auto f = frame(0x78);
    const auto name = f[0x18];
    const auto prefix = f[0x10];
    const auto port = a0 & 0xffU;
    const auto entry = port * 16 + (a1 & 0xffU);
    copy_device(*this, prefix, port);
    strcpy(name, prefix);
    strcat(name, u32(at(state_card)) + entry * entry_bytes + entry_name);
    static_cast<void>(read_file_head(name, u32(at(state_card)) + entry * head_bytes + heads));
    const auto blocks = [&] { return u8(u32(at(state_card)) + entry * head_bytes + heads + 3); };
    for (std::uint32_t block = 0; block < blocks(); ++block) {
        const auto cards = u32(at(state_card));
        put8(cards + port * 16 + u32(cards + listed_count) + position_entry, a1 + port * 16);
        put32(cards + listed_count, u32(cards + listed_count) + 1);
    }
}

// 801c9270(port): mark this game's files among the port's 15 positions:
// entries whose name starts with the 12-byte game prefix (card state +
// 4fce). Each keeps the address of its payload head (801ea6f4) and marks
// its file digit (payload + 23) used (801ea6d0).
void Overlay::mark_game_files(std::uint32_t a0) {
    const auto cards = u32(at(state_card));
    for (std::uint32_t n = 0; n < 16; ++n)
        put8(cards + a0 * 16 + n + game_file, 0);
    const auto first = a0 * 16;
    for (auto position = first; static_cast<std::int32_t>(position) <
                                static_cast<std::int32_t>(first + 15);
         ++position) {
        const auto entry = u8(cards + position + position_entry);
        bool matches = true;
        for (std::uint32_t k = 0; k < 12; ++k)
            if (u8(cards + entry * entry_bytes + entry_name + k) != u8(cards + game_prefix + k)) {
                matches = false;
                break;
            }
        if (!matches)
            continue;
        put8(cards + position + game_file, 1);
        const auto head = cards + u8(cards + position + position_entry) * head_bytes + heads;
        put32(last_payload, head + 0x100);
        put8(used_digits + first + u8(head + 0x123), 1);
    }
}

// 801c93a8: refresh the cards before a save or load. With no card in
// either port: close the message (801d32b4), show message 23, and when
// still none, wait a second and report 1; otherwise reinitialize the card
// events (801d9b08) and forget the presence and listings. With cards: for
// each port whose file count changed, reinitialize, erase the temporary
// file, and read each file head (801c90b0) and its icon (801e78c8) while
// the cards stay as they were (a change stops with result 0 and skips the
// rest); then mark this game's files (801c9270) and place the cursor
// marker on the cursor slot. Menu sounds are off meanwhile; the poll period
// becomes 1eh. Returns 1 when there is no card.
std::uint32_t Overlay::refresh_cards() {
    const auto stack_frame = enter(0xe8);
    auto f = frame(0xe8);
    const auto device = f[0x10];
    const auto party = [&] { return u32(at(state_party)); };
    const auto cards = u32(at(state_card));
    put8(cards_reset, 0);
    put8(cards + poll_mode, 2);
    menu_frame();
    std::uint32_t no_card = 0;
    if (u16(cards + port_present) == 0) {
        close_message();
        put8(party() + message_pending, 0);
        put8(party() + file_details_shown, 0);
        menu_frame();
        show_message(no_card_message);
        put8(at(load_state), 1);
        put8(party() + cursor_active, 0);
        put32(cards + shown_slot, 0xff);
        put32(cards + cursor_slot, 0);
        if (u16(cards + port_present) == 0) {
            put8(cards + port_files_seen + 1, 0xff);
            put8(cards + port_files_seen, 0xff);
            menu_frame();
            for (std::uint32_t i = 0; i < 0x3b; ++i)
                vsync();
            no_card = 1;
        }
        if (no_card == 0) {
            restart_card_access();
            put8(cards + presence_seen + 1, 0xff);
            put8(cards + presence_seen, 0xff);
            put8(cards + port_scanned, 0);
            put8(cards + port_scanned + 1, 0);
            put8(at(card_poll_timer), 0x3c);
            menu_frame();
            put8(cards_reset, 1);
        }
        close_message();
    }
    put8(poll_period, 1);
    const std::array<std::uint32_t, 2> present{u8(cards + port_present), u8(cards + port_present + 1)};
    // With no card the original tests the caller's S3 here (never set in
    // this function); both of its outcomes reach the end: the cursor check
    // below finds the party's cursor flag cleared above.
    if (no_card == 0) {
        std::uint32_t changed = 0; // S3
        put8(at(sounds), 0);
        for (std::uint32_t port = 0; port < 2; ++port) {
            const auto files = u8(cards + port + port_files);
            changed = 0;
            if (files != u8(cards + port + port_files_seen) && files != 0) {
                restart_card_access();
                for (std::uint32_t n = 0; n < 16; ++n)
                    put8(cards + port * 16 + n + position_entry, 0xff);
                copy_device(*this, device, port);
                strcat(device, temp_suffix);
                static_cast<void>(card_erase(*this, device));
                put32(cards + listed_count, 0);
                put32(port_blocks + port * 4, 0);
                std::uint32_t n = 0;
                const auto count = u8(cards + port + port_files);
                if (count != 0) {
                    for (;;) {
                        if (u8(cards + port + port_present) == 0 ||
                            present[0] != u8(cards + port_present) ||
                            present[1] != u8(cards + port_present + 1)) {
                            changed = 2;
                            break;
                        }
                        load_file_head(port, n);
                        const auto entry = port * 16 + n;
                        load_file_icon(entry);
                        put8(cards + entry * entry_bytes + entry_icon_shown, 1);
                        menu_frame();
                        if (u8(cards + port + port_scanned) == 0) {
                            changed = port + 1;
                            break;
                        }
                        ++n;
                        if (static_cast<std::int32_t>(n) >=
                            static_cast<std::int32_t>(u8(cards + port + port_files)))
                            break;
                    }
                }
                if (changed != 0)
                    break; // S2 = 2 ends the port loop
                for (; n < 16; ++n)
                    put8(cards + (port * 16 + n) * entry_bytes + entry_icon_shown, 0);
                put8(cards + port + port_files_seen, u8(cards + port + port_files));
            }
            if (u8(cards + port + port_files) == 0) {
                for (std::uint32_t n = 0; n < 16; ++n)
                    put8(cards + (port * 16 + n) * entry_bytes + entry_icon_shown, 0);
                put8(cards + port + port_files_seen, 0xff);
            }
        }
        if (changed == 0) {
            mark_game_files(0);
            mark_game_files(1);
            // S6 (always 0 here) would set the party's details flag.
            const auto marks = u32(at(markers));
            if (u8(party() + cursor_active) != 0 && u8(marks + cursor_shown) != 0) {
                // The marker record and cursor slot are re-read for each corner.
                const auto record = [&] {
                    const auto m = u32(at(markers));
                    return m + u8(m + cursor_record) * 0x28;
                };
                const auto x = [&] {
                    return u16(position_x + u32(slot_positions + u32(cards + cursor_slot) * 4) * 4);
                };
                const auto y = [&] {
                    return u16(position_y + u32(slot_positions + u32(cards + cursor_slot) * 4) * 4);
                };
                put16(record() + 0x08, x() + 8);
                put16(record() + 0x0a, y() - 6);
                put16(record() + 0x10, x() + 0x18);
                put16(record() + 0x12, y() - 6);
                put16(record() + 0x18, x() + 8);
                put16(record() + 0x1a, y() + 10);
                put16(record() + 0x20, x() + 0x18);
                put16(record() + 0x22, y() + 10);
            }
        }
    }
    put8(at(sounds), 1);
    put8(poll_period, 0x1e);
    return no_card;
}

// 801c9bcc(mode): whether the cursor slot suits `mode`: 0 a file is there,
// 1 this game's file is there (both need files listed: card state + 4f8a/
// 4f8b nonzero), 2 its card is present; other modes are always suitable.
// Suitable sets menu state + 4d8 to 2. Returns 1 or 0.
std::uint32_t Overlay::cursor_slot_suits(std::uint32_t a0) {
    const auto cards = u32(at(state_card));
    const auto slot = u32(cards + cursor_slot);
    const auto position = [&] { return u32(slot_positions + slot * 4); };
    const auto port_of = [&](std::uint32_t m) { return signed_div(m, 16); };
    const auto listed = [&] { return (u32(cards + port_scanned) & 0xffff0000U) != 0; };
    std::uint32_t valid = 1;
    const auto mode = static_cast<std::int32_t>(a0);
    if (mode == 0 || mode == 1) {
        if (!listed()) {
            valid = 0;
        } else {
            const auto m = position();
            if (u8(cards + port_of(m) + port_present) == 0 ||
                u8(cards + m + position_entry) == 0xff ||
                (mode == 1 && u8(cards + m + game_file) == 0))
                valid = 0;
        }
    } else if (mode == 2) {
        if (u8(cards + port_of(position()) + port_present) == 0)
            valid = 0;
    }
    if (valid != 0)
        put8(at(load_state), 2);
    return valid;
}

// 801c9d34(mode): the first slot of the present ports that suits `mode` (as
// 801c9bcc), or ff; modes 0 and 1 stop at once when no files are listed,
// other modes find none. Sets menu state + 4d8 to 2.
std::uint32_t Overlay::find_suitable_slot(std::uint32_t a0) {
    const auto cards = u32(at(state_card));
    std::uint32_t found = 0xff;
    const std::uint32_t first = u8(cards + port_present) == 0 ? 15 : 0;
    const std::uint32_t end = u8(cards + port_present + 1) == 0 ? 15 : 30;
    const auto mode = static_cast<std::int32_t>(a0);
    for (auto slot = first; slot < end; ++slot) {
        if (mode != 0 && mode != 1 && mode != 2)
            continue;
        const auto m = u32(slot_positions + slot * 4);
        if (mode != 2 && (u32(cards + port_scanned) & 0xffff0000U) == 0)
            break;
        if (u8(cards + signed_div(m, 16) + port_present) == 0)
            continue;
        if (mode != 2 && (u8(cards + m + position_entry) == 0xff ||
                          (mode == 1 && u8(cards + m + game_file) == 0)))
            continue;
        found = slot;
        break;
    }
    put8(at(load_state), 2);
    return found;
}

// 801c9ef4(mode, slot): move the cursor a row down (slot + 3) for `mode`:
// 0 to the next slot below holding a file, else to the first file after
// slot + 3; 1 the same for this game's files; 2 a row down when that card
// is present (past the last row: slot + 4 when both it and slot + 3 are
// below 30).
void Overlay::card_cursor_down(std::uint32_t a0, std::uint32_t a1) {
    const auto cards = u32(at(state_card));
    auto slot = static_cast<std::int32_t>(a1);
    const auto mode = static_cast<std::int32_t>(a0);
    const auto position = [&](std::int32_t s) {
        return u32(slot_positions + static_cast<std::uint32_t>(s) * 4);
    };
    const auto suits = [&](std::int32_t s) {
        const auto m = position(s);
        return u8(cards + m + position_entry) != 0xff &&
               (mode == 0 || u8(cards + m + game_file) != 0);
    };
    const auto set_cursor = [&](std::int32_t s) {
        put32(cards + cursor_slot, static_cast<std::uint32_t>(s));
    };
    if (mode == 0 || mode == 1) {
        for (; slot + 3 < 30; slot += 3)
            if (suits(slot + 3)) {
                set_cursor(slot + 3);
                return;
            }
        const auto from = static_cast<std::int32_t>(u32(cards + cursor_slot));
        if (from + 3 >= 30 || from + 4 >= 30)
            return;
        for (auto s = from + 4; s < 30; ++s)
            if (suits(s)) {
                set_cursor(s);
                return;
            }
    } else if (mode == 2) {
        const auto below = slot + 3;
        if (u8(cards + static_cast<std::uint32_t>(below / 15) + port_present) == 0)
            return;
        if (below < 30) {
            set_cursor(below);
            return;
        }
        const auto from = static_cast<std::int32_t>(u32(cards + cursor_slot));
        if (from + 3 < 30 && from + 4 < 30)
            set_cursor(from + 4);
    }
}

// 801ca750(mode): the file screen's cursor input (menu state + 325): code 0
// moves down (801c9ef4), 1-3 the other directions (801ca480, 801ca1d4,
// 801ca5f0), 4 confirms (1) and 5 cancels (2); others return 0. A new
// cursor slot shows its file's details (801e781c).
std::uint32_t Overlay::card_cursor_input(std::uint32_t a0) {
    const auto stack_frame = enter(0x18);
    std::uint32_t result = 0;
    const auto cards = u32(at(state_card));
    switch (u8(at(input_code))) {
    case 0:
        card_cursor_down(a0, u32(cards + cursor_slot));
        break;
    case 1:
        missing("menu_card_cursor", 0x801ca80c, "symbol:menu-card-cursor-801ca480",
                "The cursor move 801ca480 is outside the census");
    case 2:
        missing("menu_card_cursor", 0x801ca7e4, "symbol:menu-card-cursor-801ca1d4",
                "The cursor move 801ca1d4 is outside the census");
    case 3:
        missing("menu_card_cursor", 0x801ca834, "symbol:menu-card-cursor-801ca5f0",
                "The cursor move 801ca5f0 is outside the census");
    case 4:
        result = 1;
        break;
    case 5:
        result = 2;
        break;
    default:
        break;
    }
    if (u32(cards + cursor_slot) != u32(cards + shown_slot)) {
        const auto m = u32(slot_positions + u32(cards + cursor_slot) * 4);
        show_file_details(u8(cards + m + position_entry), u8(cards + m + game_file));
        put32(cards + shown_slot, u32(cards + cursor_slot));
    }
    return result;
}

// 801ca8c0(digit): the save header's title (card state + 4b98): the title
// template (801c50e0, 1bh bytes), then the file number digit + 1 as two
// full-width digits, a full-width space and the suffix (card state + 4ffc).
void Overlay::build_save_title(std::uint32_t a0) {
    const auto number = (a0 & 0xffU) + 1;
    const auto header = u32(at(state_card)) + card_header;
    for (std::uint32_t i = 0; i < 0x1b; ++i)
        put8(header + 4 + i, u8(title_template + i));
    const auto tens = signed_div(number, 10);
    put8(header + 0x1e, 0x82);
    put8(header + 0x1f, tens + 0x4f);
    put8(header + 0x20, 0x82);
    put8(header + 0x21, number - tens * 10 + 0x4f);
    put8(header + 0x22, 0x81);
    put8(header + 0x23, 0x40);
    put8(header + 0x24, 0);
    put8(header + 0x25, 0);
    strcat(header + 4, u32(at(state_card)) + title_suffix);
}

// 801caa38(watch): the yes/no choice: frames until confirm (4) or cancel
// (5); left/right (2 / 0) move the highlight (1 yes, 0 no). Without `watch`
// the choice waits b4h frames at most while the input stays 8 (none) and
// returns at once on any other input. With `watch` and 801e977a set, a card
// change clears that port's listing and cancels (801e9778, 801ea8fc set).
// Returns 1 for yes.
std::uint32_t Overlay::confirm_choice(std::uint32_t a0) {
    const auto stack_frame = enter(0x38);
    const auto cards = u32(at(state_card));
    const std::array<std::uint32_t, 2> present{u8(cards + port_present), u8(cards + port_present + 1)};
    const auto marks = [&] { return u32(at(markers)); };
    if (u8(watch_cards) != 0)
        put8(cards + poll_mode, 2);
    put8(card_cancelled, 0);
    std::uint32_t yes = 0;
    std::uint32_t waits = 0xb4;
    const auto watch = a0 & 0xffU;
    for (bool again = true; again;) {
        if (watch == 0) {
            put8(marks() + yes_marker, 0);
            put8(marks() + no_marker, 0);
            if (u8(at(input_code)) != 8)
                break;
            waits = (waits - 1) & 0xffU;
            if (waits == 0)
                break;
        }
        menu_frame();
        if (watch != 0 && u8(watch_cards) != 0) {
            for (std::uint32_t port = 0; port < 2; ++port)
                if (present[port] != u8(cards + port_present + port)) {
                    put8(cards + port_scanned + port, 0);
                    put8(at(input_code), 5);
                    put8(card_changed, 1);
                }
        }
        switch (u8(at(input_code))) {
        case 2:
            put8(marks() + yes_marker, 1);
            yes = 1;
            put8(marks() + no_marker, 0);
            break;
        case 0:
            put8(marks() + yes_marker, 0);
            yes = 0;
            put8(marks() + no_marker, 1);
            break;
        case 4:
            again = false;
            break;
        case 5:
            yes = 0;
            put8(card_cancelled, 1);
            again = false;
            break;
        default:
            break;
        }
    }
    put8(marks() + yes_marker, 0);
    put8(marks() + no_marker, 0);
    put8(cards + poll_mode, 0);
    return yes;
}

// 801cacf8(message, second, watch): show `message` (801d2f4c) and ask yes/no
// (801caa38(watch)); when `second` is not ff and the answer was yes, ask
// again with `second`. Returns the last answer.
std::uint32_t Overlay::ask_confirmation(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) {
    const auto stack_frame = enter(0x28);
    const auto watch = a2 & 0xffU;
    show_message(a0 & 0xffU);
    put8(u32(at(markers)) + no_marker, 1);
    auto yes = confirm_choice(watch);
    close_message();
    if ((a1 & 0xffU) != 0xff && (yes & 0xffU) != 0) {
        show_message(a1 & 0xffU);
        put8(u32(at(markers)) + no_marker, 1);
        yes = confirm_choice(watch);
        close_message();
    }
    return yes & 0xffU;
}

// 801cadb0: reset the cursor: slot 0 (15 when only port 1 has a card), no
// details shown.
void Overlay::reset_card_cursor() {
    const auto cards = u32(at(state_card));
    put32(cards + shown_slot, 0xff);
    put32(cards + cursor_slot, 0);
    if (u8(cards + port_present) == 0 && u8(cards + port_present + 1) != 0)
        put32(cards + cursor_slot, 15);
}

// 801cae08(mode): the card access indicator. 0 removes it (a frame, then
// its block is released); 1 builds it: a 7bc-byte block with a flat quad
// per buffer (a0, a0, 0), sprites 160 and 161 at (a0, 64), the build buffer
// at +7b8, +7b4 = 8, and effects e0, e1 and 8f of the menu bank (80039e60);
// 2 closes it (continued at 801cb0a8).
void Overlay::card_access_indicator(std::uint32_t a0) {
    const auto stack_frame = enter(0x30);
    switch (a0 & 0xffU) {
    case 0: {
        const auto party = u32(at(state_party));
        if (u8(party + access_state) == 0)
            return;
        put8(party + access_state, 0);
        menu_frame();
        release(u32(at(access_sprite)), 0x801cae94);
        return;
    }
    case 1: {
        const auto block = allocate(0x7bc, 0, 0x801caea8);
        put32(at(access_sprite), block);
        bzero(block, 0x7bc);
        const auto quad = [&] { return u32(at(access_sprite)) + u32(at(buffer_index)) * 0x18; };
        set_poly_f4(quad());
        put8(quad() + 4, 0xa0);
        put8(quad() + 5, 0xa0);
        put8(quad() + 6, 0);
        for (const auto [id, packets] : {std::pair{0x160U, 0x30U}, std::pair{0x161U, 0x3f0U}})
            static_cast<void>(resident::sheet_quads(*this, u32(at(sprite_sheet)), id,
                                                    u32(at(access_sprite)) + packets,
                                                    u32(at(buffer_index)), 0xa0, 0x64, 0x1000));
        put8(u32(at(access_sprite)) + 0x7b8, u8(at(buffer_index)));
        put8(u32(at(state_party)) + access_state, 1);
        put32(u32(at(access_sprite)) + 0x7b4, 8);
        for (const auto [effect, site] :
             {std::pair{0xe0U, 0x801cb038U}, std::pair{0xe1U, 0x801cb060U},
              std::pair{0x8fU, 0x801cb088U}})
            menu_effect(*this, (u16(u32(at(effect_bank)) + 0x14) << 16) | effect, site);
        return;
    }
    case 2:
        close_access_indicator(state());
        return;
    default:
        return;
    }
}

// 801cb0a8: the tail of 801cae08 mode 2 (A0 is the menu state pointer
// there): when the indicator is shown, set +7b0 to 100h, make the shown
// buffer's quad (0, a0, 0) and mark it closing (2).
void Overlay::close_access_indicator(std::uint32_t a0) {
    if (u8(u32(a0 + state_party) + access_state) == 0)
        return;
    put32(u32(a0 + access_sprite) + 0x7b0, 0x100);
    const auto quad = [&] {
        const auto block = u32(a0 + access_sprite);
        return block + u8(block + 0x7b8) * 0x18;
    };
    put8(quad() + 4, 0);
    put8(quad() + 5, 0xa0);
    put8(quad() + 6, 0);
    put8(u32(at(state_party)) + access_state, 2);
}

// ---- Save and load --------------------------------------------------------

// 801cb184: decode the 31 names of the game data in place (80033b34 through
// a 20-byte buffer on the stack, copied back whole).
void Overlay::decode_game_names() {
    const auto stack_frame = enter(0x58);
    auto f = frame(0x58);
    const auto codes = f[0x10];
    const auto text = f[0x28];
    for (std::uint32_t i = 0; i < names; ++i) {
        const auto name = game + i * name_size;
        const auto count = copy_name_codes(*this, name, codes);
        decode_codes(*this, codes, text, count);
        for (std::uint32_t k = 0; k < name_size; ++k)
            put8(name + k, u8(text + k));
    }
}

// 801cb28c(payload): apply a loaded payload: restore the game data
// (801e4d10 with the data table directory), the play counter (80059488)
// from payload + 0, the globals 8005a3a0 from game + 2324, and decode the
// names (801cb184).
void Overlay::apply_loaded_payload(std::uint32_t a0) {
    const auto stack_frame = enter(0x18);
    restore_payload_game_data(a0, u32(at(state_tables)));
    program.resident.pad.vsyncs = u32(a0);
    for (std::uint32_t i = 0; i < 0x20; i += 2)
        put16(saved_globals + i, u16(game_globals + i));
    decode_game_names();
}

// 801cb304: the load. Refresh the cards (801c93a8; no card returns), find a
// slot with this game's file (801c9bcc / 801c9d34; none: message 62 and
// return 0), then run the cursor (801ca750(1)) until cancel (return 0) or
// confirm: ask (message 65); on yes open the file (mode 1), read its block
// count's bytes in 100h chunks into a new 2100h block, and when the payload's
// eight-bit sum matches its last byte apply it (801c72bc(1), 801cb28c,
// 801c72bc(11h)): effect 34 and message 5c end the load; a mismatch shows
// message 3e and loops. Returns 1 unless cancelled or without data.
std::uint32_t Overlay::load_game() {
    const auto stack_frame = enter(0x88);
    auto f = frame(0x88);
    const auto name = f[0x10];
    const auto party = [&] { return u32(at(state_party)); };
    const auto marks = [&] { return u32(at(markers)); };
    const auto cards = u32(at(state_card));
    std::uint32_t result = 1; // sp+58
    bool first = true;        // sp+50
    bool again = true;        // S7
    reset_card_cursor();
    while (again) {
        if ((refresh_cards() & 0xffU) != 0)
            break;
        if (first) {
            if (u8(party() + message_pending) != 0)
                close_message();
            first = false;
            put8(marks() + markers_140, 1);
        }
        if (cursor_slot_suits(1) == 0) {
            const auto slot = find_suitable_slot(1);
            put32(cards + cursor_slot, slot);
            if (slot == 0xff) {
                put8(party() + cursor_active, 0);
                put8(party() + file_details_shown, 0);
                put8(at(load_state), 1);
                result = 0;
                put8(party() + file_details_shown, 0);
                static_cast<void>(ask_confirmation(no_data_message, 0xff, 0));
                break;
            }
        }
        put8(party() + cursor_active, 1);
        const auto choice = card_cursor_input(1);
        if (choice == 2) {
            again = false;
            result = 0;
            put8(at(sounds), 1);
            continue;
        }
        if (choice != 1)
            continue;
        put8(marks() + cursor_shown, 0);
        put8(cards + poll_mode, 0);
        const std::uint32_t port = static_cast<std::int32_t>(u32(cards + cursor_slot)) < 15 ? 0 : 1;
        copy_device(*this, name, port);
        const auto position = u32(slot_positions + u32(cards + cursor_slot) * 4);
        strcat(name, cards + u8(cards + position + position_entry) * entry_bytes + entry_name);
        if ((ask_confirmation(0x65, 0xff, 1) & 0xffU) == 0) {
            put32(cards + shown_slot, 0xff);
            put8(marks() + cursor_shown, 1);
            continue;
        }
        show_message(0x3b);
        put8(at(sounds), 0);
        std::uint32_t fd = 0; // S2
        for (std::uint32_t tries = 5;;) {
            fd = card_open(*this, name, 1);
            if (fd == none)
                retry_wait(0x801cb60c);
            if (fd != 0 || ((--tries) & 0xffU) == 0)
                break;
        }
        if (fd != 0) {
            put8(party() + cursor_active, 0);
            const auto buffer = allocate(0x2100, 1, 0x801cb648);
            put8(party() + file_details_shown, 0);
            card_access_indicator(1);
            auto chunk = buffer;
            for (std::uint32_t done = 0;;) {
                menu_frame();
                if (card_read(*this, fd, chunk, 0x100) != 0x100)
                    retry_wait(0x801cb698);
                done += 0x100;
                chunk += 0x100;
                if (static_cast<std::int32_t>(done) >=
                    static_cast<std::int32_t>(u8(cards + card_header + 3) << 13))
                    break;
            }
            static_cast<void>(card_close(*this, fd));
            Menu context{*program.menu, program.resident.sound};
            if (check_loaded(context, buffer).decision == LoadDecision::accepted) {
                load_menu_data_set(1);
                apply_loaded_payload(buffer + header_size);
                load_menu_data_set(0x11);
            } else {
                fd = 0;
            }
            release(buffer, 0x801cb728);
        }
        card_access_indicator(2);
        close_message();
        if (fd != 0) {
            again = false;
            put8(at(sounds), 1);
            play_menu_sound(0x34);
            put8(at(sounds), 0);
            static_cast<void>(ask_confirmation(0x5c, 0xff, 0));
        } else {
            static_cast<void>(ask_confirmation(checksum_mismatch_message, 0xff, 0));
            put8(cards + poll_mode, 2);
        }
        card_access_indicator(0);
        put8(cards + port_scanned, 0);
        put8(cards + port_scanned + 1, 0);
        put8(cards + port_files_seen, 0xff);
        put8(cards + port_files_seen + 1, 0xff);
        put8(marks() + cursor_shown, 1);
    }
    put8(cards + poll_mode, 1);
    put8(at(b_334), 1);
    return result & 0xffU;
}

// 801cb8ac(port): the unformatted-card question: message 29h + 3 * port,
// then frames while the input stays 8 and the cards stay as they were; a
// card change returns 0, otherwise the answer to message 2f (801cacf8).
std::uint32_t Overlay::ask_format_card(std::uint32_t a0) {
    const auto stack_frame = enter(0x28);
    const auto cards = u32(at(state_card));
    const std::array<std::uint32_t, 2> present{u8(cards + port_present), u8(cards + port_present + 1)};
    show_message(((a0 & 0xffU) * 3 + 0x29) & 0xffU);
    put8(at(input_code), 8);
    put8(cards + poll_mode, 2);
    bool changed = false;
    while (u8(at(input_code)) == 8) {
        menu_frame();
        if (present[0] != u8(cards + port_present) || present[1] != u8(cards + port_present + 1)) {
            changed = true;
            break;
        }
    }
    close_message();
    if (changed)
        return 0;
    return ask_confirmation(0x2f, 0xff, 1) & 0xffU;
}

// 801cb9e8(port, existing): the file digit (choose_file_digit over the
// port's used digits 801ea6d0).
std::uint32_t Overlay::choose_save_digit(std::uint32_t a0, std::uint32_t a1) {
    std::array<std::uint8_t, 16> used{};
    for (std::uint32_t i = 0; i < used.size(); ++i)
        used[i] = static_cast<std::uint8_t>(u8(used_digits + (a0 & 0xffU) * 16 + i));
    return choose_file_digit(used, static_cast<std::uint8_t>(a1 & 0xffU));
}

// 801cba4c(payload, port, digit): fill the zeroed payload: the globals
// 8005a3a0 into game + 2324, the party summary (per slot: id or ff, and
// the character's HP, maximum HP, level and three bytes), +1f 0, the digit
// at +23, the play counter at +0; each name encoded (80033c20) in place
// through a zeroed 20-byte buffer; the disc word (game + 19d4: 1 when
// 801e96a5 is set, else the current disc - 1); the game data copy
// (801e4a28) and the names decoded back (801cb184). A1 is unused.
void Overlay::build_save_payload(std::uint32_t a0, std::uint32_t, std::uint32_t a2) {
    const auto stack_frame = enter(0x58);
    auto f = frame(0x58);
    const auto text = f[0x10];
    const auto codes = f[0x28];
    const auto payload = a0;
    for (std::uint32_t i = 0; i < 0x20; i += 2)
        put16(game_globals + i, u16(saved_globals + i));
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        const auto member = [&] { return u8(u32(at(state_party)) + 0x30 + slot); };
        if (member() == 0xff) {
            put8(payload + 0x1c + slot, 0xff);
            continue;
        }
        put8(payload + 0x1c + slot, member());
        const auto record = [&] { return character_record(member()); };
        put16(payload + 4 + slot * 2, u16(record() + 0x4c));
        put16(payload + 0xa + slot * 2, u16(record() + 0x4e));
        put8(payload + 0x10 + slot, u8(record() + 0x50));
        put8(payload + 0x13 + slot, u8(record() + 0x52));
        put8(payload + 0x16 + slot, u8(record() + 0x62));
        put8(payload + 0x19 + slot, u8(record() + 0x63));
    }
    put8(payload + 0x1f, 0);
    put8(payload + 0x23, a2);
    put32(payload, program.resident.pad.vsyncs); // 80059488
    for (std::uint32_t i = 0; i < names; ++i) {
        const auto name = game + i * name_size;
        for (std::uint32_t k = 0; k < name_size; ++k) {
            put8(text + k, u8(name + k));
            put8(codes + k, 0);
        }
        const auto encoded = encode_text(codec(*this), bytes_at(*this, text, name_size));
        if (encoded.codes.size() > name_size / 2)
            throw MenuError("An encoded name overruns 801cba4c's buffer");
        for (std::uint32_t k = 0; k < encoded.codes.size(); ++k)
            put16(codes + k * 2, encoded.codes[k]);
        for (std::uint32_t k = 0; k < name_size; ++k)
            put8(name + k, u8(codes + k));
    }
    put16(game_disc, u8(disc_override) != 0 ? 1U : current_disc() - 1);
    store_payload_game_data(payload);
    decode_game_names();
}

// 801cbd90(kind): the save; its first instructions set up the frame
// 801cbdbc continues in (existing digit ff, first pass 1, A3 1).
std::uint32_t Overlay::save_game(std::uint32_t a0) {
    const auto stack_frame = enter(0x100);
    return save_game_body(a0, 0, 0, 1);
}

// 801cbdbc: the save after 801cbd90's prologue (A3 is the first loop flag).
// Refresh the cards (801c93a8; no card returns 1), find a slot whose card is
// present (none: message ac), then run the cursor (801ca750(2)) until cancel
// or confirm. An unformatted card asks to format it (801cb8ac, message 26,
// format; done: message 5c; failed: the save ends). An empty slot asks
// message 5f; another game's file refuses (message c4); this game's file
// asks message 38, then erases it and keeps its digit. The save proper:
// digit (801cb9e8), final name device + prefix + digit, temporary name
// device + suffix; erase the temporary file, create it (block count << 16 |
// 200h), reopen it for writing, write the header template with the title
// (801ca8c0), then the payload built by 801cba4c and sealed with its
// checksum in 100h chunks behind the access indicator, close, and rename it
// to the final name. Effect 34 and message 5c report success, message 35
// failure; then the listing is refreshed. With `kind` 0 one save ends the
// loop. Returns 1 when there was no card.
std::uint32_t Overlay::save_game_body(std::uint32_t a0, std::uint32_t, std::uint32_t,
                                 std::uint32_t a3) {
    auto f = frame(0x100);
    const auto final_name = f[0x10];
    const auto temp_name = f[0x50];
    const auto device = f[0x90];
    const auto digit_text = f[0x98];
    const auto created = f[0xb8];
    const auto party = [&] { return u32(at(state_party)); };
    const auto marks = [&] { return u32(at(markers)); };
    const auto cards = u32(at(state_card));
    const auto kind = a0 & 0xffU;
    std::uint32_t existing = 0xff; // sp+d0
    bool first = true;             // sp+c0
    auto again = a3;               // sp+b0
    std::uint32_t result = 0;      // sp+c8
    reset_card_cursor();
    do {
        if ((refresh_cards() & 0xffU) != 0) {
            result = 1;
            break;
        }
        if (first) {
            if (u8(party() + message_pending) != 0)
                close_message();
            first = false;
            put8(marks() + markers_140, 1);
        }
        if (cursor_slot_suits(2) == 0) {
            const auto slot = find_suitable_slot(2);
            put32(cards + cursor_slot, slot);
            if (slot == 0xff) {
                static_cast<void>(ask_confirmation(0xac, 0xff, 0));
                break;
            }
        }
        put8(party() + cursor_active, 1);
        const auto choice = card_cursor_input(2);
        if (choice == 2) {
            again = 0;
            continue;
        }
        if (choice != 1)
            continue;
        put8(marks() + cursor_shown, 0);
        put8(cards + poll_mode, 0);
        bool proceed = true; // S0
        const std::uint32_t port = static_cast<std::int32_t>(u32(cards + cursor_slot)) < 15 ? 0 : 1;
        copy_device(*this, device, port);
        if (u32(cards + port * 4 + port_status) == 0xfffffffeU) {
            if ((ask_format_card(port) & 0xffU) == 0) {
                put8(marks() + cursor_shown, 1);
                continue;
            }
            show_message(0x26);
            if (bios(*this, "format").format(text_at(*this, device)) != 0) { // 80040574
                close_message();
                static_cast<void>(ask_confirmation(0x5c, 0xff, 0));
            } else {
                close_message();
                proceed = false;
                again = 0;
            }
        }
        if (proceed) {
            const auto position = [&] { return u32(slot_positions + u32(cards + cursor_slot) * 4); };
            const auto entry = [&] { return u8(cards + position() + position_entry); };
            if (entry() == 0xff) {
                if ((ask_confirmation(0x5f, 0xff, 1) & 0xffU) == 0) {
                    proceed = false;
                    put32(cards + shown_slot, 0xff);
                }
            } else if (u8(cards + position() + game_file) == 0) {
                static_cast<void>(ask_confirmation(0xc4, 0xff, 0));
                put8(marks() + cursor_shown, 1);
                continue;
            } else if ((ask_confirmation(0x38, 0xff, 1) & 0xffU) == 0) {
                proceed = false;
                put32(cards + shown_slot, 0xff);
            } else {
                strcpy(temp_name, device);
                strcat(temp_name, cards + entry() * entry_bytes + entry_name);
                static_cast<void>(card_erase(*this, temp_name));
                existing = u8(cards + entry() * head_bytes + heads + 0x123);
            }
        }
        if (proceed) {
            put8(party() + cursor_active, 0);
            put8(party() + file_details_shown, 0);
            put32(cards + shown_slot, 0xff);
            put8(cards + port_files_seen, 0xff);
            put8(cards + port_files_seen + 1, 0xff);
            const auto digit = choose_save_digit(port, existing);
            put8(digit_text, digit + 0x30);
            put8(digit_text + 1, 0);
            strcpy(final_name, device);
            strcpy(temp_name, device);
            strcat(final_name, cards + game_prefix);
            strcat(final_name, digit_text);
            strcat(temp_name, temp_suffix);
            show_message(0x32);
            put8(at(sounds), 0);
            static_cast<void>(card_erase(*this, temp_name));
            std::uint32_t fd = 0; // S1
            for (std::uint32_t tries = 3;;) {
                fd = card_open(*this, temp_name, (u8(cards + card_header + 3) << 16) | 0x200);
                if (fd == none)
                    retry_wait(0x801cc2bc);
                put32(created, fd);
                if (fd != 0 || ((--tries) & 0xffU) == 0)
                    break;
            }
            static_cast<void>(card_close(*this, u32(created)));
            if (fd != 0) {
                for (std::uint32_t tries = 3;;) {
                    fd = card_open(*this, temp_name, 2);
                    if (fd == none)
                        retry_wait(0x801cc31c);
                    if (fd != 0)
                        break;
                    if (((--tries) & 0xffU) == 0) {
                        static_cast<void>(card_erase(*this, temp_name));
                        break;
                    }
                }
                menu_frame();
            }
            if (fd != 0) {
                build_save_title(digit & 0xffU);
                if (card_write(*this, fd, cards + card_header, 0x100) == none)
                    retry_wait(0x801cc38c);
                const auto payload = allocate(payload_size, 1, 0x801cc3f8);
                bzero(payload, static_cast<std::int32_t>(payload_size));
                build_save_payload(payload, port, digit & 0xffU);
                Menu context{*program.menu, program.resident.sound};
                static_cast<void>(seal_payload(context, payload));
                card_access_indicator(1);
                auto chunk = payload;
                for (std::uint32_t written = 0x100;;) {
                    menu_frame();
                    if (card_write(*this, fd, chunk, 0x100) != 0x100)
                        retry_wait(0x801cc47c);
                    written += 0x100;
                    chunk += 0x100;
                    if (static_cast<std::int32_t>(written) >=
                        static_cast<std::int32_t>(u8(cards + card_header + 3) << 13))
                        break;
                }
                static_cast<void>(card_close(*this, fd));
                strcpy(temp_name, device);
                strcat(temp_name, temp_suffix);
                if (bios(*this, "rename")
                        .rename(text_at(*this, temp_name), text_at(*this, final_name)) == 0)
                    retry_wait(0x801cc50c); // 800405a4
                release(payload, 0x801cc534);
            }
            close_message();
            card_access_indicator(2);
            if (fd != 0) {
                put8(at(sounds), 1);
                play_menu_sound(0x34);
                put8(at(sounds), 0);
                static_cast<void>(ask_confirmation(0x5c, 0xff, 0));
            } else {
                static_cast<void>(ask_confirmation(0x35, 0xff, 0));
            }
            card_access_indicator(0);
            put8(cards + poll_mode, 2);
            while (u8(at(card_poll_timer)) != 1)
                menu_frame();
            put8(cards + port_scanned, 0);
            put8(cards + port_scanned + 1, 0);
        }
        put8(marks() + cursor_shown, 1);
        put8(at(sounds), 1);
        if (kind == 0)
            again = 0;
    } while (again != 0);
    put8(cards + poll_mode, 1);
    put8(at(b_334), 1);
    return result & 0xffU;
}

// 801cd710(kind): the file command (menu state + 338) after 801d22f4(0):
// 0 delete (801cd2ac), 1 copy (801cc6d8), 2 load (801cb304) on the title
// file screen (80059460 == 2) or save (801cbd90(kind)); state + 334 names
// the command (7, 6, 2, 3). 801d2484 follows. Returns 0 when the command
// returned nonzero, else 1.
std::uint32_t Overlay::run_file_command(std::uint32_t a0) {
    const auto stack_frame = enter(0x20);
    set_markers(0);
    put8(u32(at(state_party)) + cursor_active, 0);
    std::uint32_t result = 1;
    std::uint32_t done = 0;
    bool ran = true;
    switch (u8(at(file_choice))) {
    case 0:
        put8(at(b_334), 7);
        missing("menu_file_delete", 0x801cd790, "symbol:menu-file-delete-801cd2ac",
                "The delete command 801cd2ac is outside the census");
    case 1:
        put8(at(b_334), 6);
        missing("menu_file_copy", 0x801cd7a4, "symbol:menu-file-copy-801cc6d8",
                "The copy command 801cc6d8 is outside the census");
    case 2:
        if (u8(menu_mode) == 2) {
            put8(at(b_334), 2);
            done = load_game();
        } else {
            put8(at(b_334), 3);
            done = save_game(a0 & 0xffU);
        }
        break;
    default:
        ran = false;
        break;
    }
    if (ran && (done & 0xffU) != 0)
        result = 0;
    clear_markers();
    return result;
}

// ---- Messages -------------------------------------------------------------

// 801d2f4c(message): open the message window (801d397c(2, 7a, 96, bc, 40,
// 1, 1, 4, 0)) and wait until it is open (+11); allocate four line records
// (80h bytes; lines 0 and 2 with a 5cah-byte image at VRAM (140, 4e + 13 *
// row pair), 3ah x 0dh, lines 1 and 3 sharing the previous image), lay out
// lines message .. message + 2 of the label text (80033728, 80034eac in
// plane line & 1, width 36h), set up their packets (801e7c50, 801e920c at
// (84, a0 + 16 * line), 801c851c), load the two images, mark the message
// open and release the images; two frames follow.
void Overlay::show_message(std::uint32_t a0) {
    const auto stack_frame = enter(0x50);
    open_window(2, 0x7a, 0x96, 0xbc, 0x40, 1, 1, 4, 0);
    const auto window = u32(at(message_window));
    while (u8(window + 0x11) == 0)
        menu_frame();
    const auto line = [&](std::uint32_t i) { return u32(at(message_lines + i * 4)); };
    for (std::uint32_t i = 0; i < 4; ++i) {
        const auto record = allocate(0x80, 0, 0x801d2ff0);
        put32(at(message_lines + i * 4), record);
        bzero(record, 0x80);
        if ((i & 1U) == 0) {
            const auto image = allocate(0x5ca, 0, 0x801d3024);
            put32(line(i) + 0x78, image);
            put16(line(i) + 0x70, 0x140);
            put16(line(i) + 0x72, (i / 2) * 13 + 0x4e);
            put16(line(i) + 0x74, 0x3a);
            put16(line(i) + 0x76, 0xd);
        } else {
            put32(line(i) + 0x78, u32(line(i - 1) + 0x78));
        }
    }
    const auto glyphs = [&](std::uint32_t window_record) { program.dialogue_glyphs(window_record); };
    for (std::uint32_t i = 0; i < 3; ++i) {
        const auto record = line(i);
        const auto text =
            resident::offset_table_entry(*this, u32(at(label_text)), (a0 & 0xffU) + i);
        put8(record + 0x7e,
             resident::layout_text_line(*this, text, u32(record + 0x78), 0x36, i & 1U, glyphs));
        set_label_packets(record, i, 0, 0);
        const auto screen_y = (i * 16 + 0xa0) & 0xffffU;
        const auto image_v = ((i / 2) * 13 + 0x4e) & 0xffU;
        set_quad_rect(record + u32(at(buffer_index)) * 0x28, 0x84, screen_y, 0, image_v,
                  u8(record + 0x7e), 0xd);
        set_screen_quad_vectors(record + 0x50, 0x84, screen_y, u8(record + 0x7e), 0xd);
        put8(record + 0x7f, 1);
        put8(record + 0x7d, u8(at(buffer_index)));
    }
    load_image(line(0) + 0x70, u32(line(0) + 0x78));
    load_image(line(2) + 0x70, u32(line(2) + 0x78));
    draw_sync();
    put8(u32(at(state_party)) + message_open, 1);
    release(u32(line(0) + 0x78), 0x801d324c);
    release(u32(line(2) + 0x78), 0x801d326c);
    menu_frame();
    menu_frame();
}

// 801d32b4: close a shown message (party +22): 801d4ea0(2), clear the open
// flag and release the four line records; then a frame.
void Overlay::close_message() {
    const auto stack_frame = enter(0x18);
    if (u8(u32(at(state_party)) + message_shown) != 0) {
        release_text_blocks(2);
        put8(u32(at(state_party)) + message_open, 0);
        for (std::uint32_t i = 0; i < 4; ++i)
            release(u32(at(message_lines + i * 4)), 0x801d3314);
    }
    menu_frame();
}

// ---- Card setup -------------------------------------------------------------

// 801d9b08: restart card access: close the events (801c8960), VSync,
// InitCARD(1), StartCARD, _bu_init, DrawSync, VSync, then open the four
// card events (done, error, timeout, new card; mode 2000h, no handler) into
// card state + 4fec.. and enable them, inside a critical section.
void Overlay::restart_card_access() {
    const auto stack_frame = enter(0x18);
    close_card_events();
    vsync();
    auto &service = bios(*this, "InitCARD");
    service.init_card(1); // 8004e794
    service.start_card(); // 8004e7e8
    service.bu_init();    // 80040464
    draw_sync();
    vsync();
    static_cast<void>(service.enter_critical_section());
    const std::array<std::uint32_t, 4> slots{io_event, error_event, timeout_event, new_card_event};
    for (std::uint32_t i = 0; i < 4; ++i)
        put32(u32(at(state_card)) + slots[i],
              service.open_event(card_class, card_specs[i], 0x2000, 0)); // 80040474
    for (const auto event : slots)
        static_cast<void>(service.enable_event(u32(u32(at(state_card)) + event))); // 800404a4
    service.exit_critical_section();
}

// 801d9c84: enter the file screen's card mode: message 20, wait for the view
// to stop (state + 329), start card access (801d9b08) with the CD callbacks
// saved and cleared (80040fb4, 80040fcc, 8004373c into 801ea718..), forget
// the presence and listings, poll now (timer 3c) and refresh (801c93a8).
// Returns 1 when there is no card; otherwise closes the message and
// returns 0.
std::uint32_t Overlay::enter_card_mode() {
    const auto stack_frame = enter(0x18);
    show_message(0x20);
    put8(u32(at(state_party)) + message_pending, 1);
    while (u8(at(view_motion)) != 0)
        menu_frame();
    put8(u32(at(state_card)) + b_4fe7, 1);
    menu_frame();
    restart_card_access();
    draw_sync();
    vsync();
    auto &service = bios(*this, "EnterCriticalSection");
    static_cast<void>(service.enter_critical_section());
    auto &cd = program.resident.cd;
    put32(saved_callbacks, cd.ready_callback); // 80040fb4(0): 800564a8
    cd.ready_callback = 0;
    put32(saved_callbacks + 4, cd.sync_callback); // 80040fcc(0): 800564ac
    cd.sync_callback = 0;
    put32(saved_callbacks + 8, u32(cd_read_callback)); // 8004373c(0)
    put32(cd_read_callback, 0);
    service.exit_critical_section();
    const auto cards = u32(at(state_card));
    put8(cards + presence_seen + 1, 0xff);
    put8(cards + presence_seen, 0xff);
    put8(cards + port_scanned, 0);
    put8(cards + port_scanned + 1, 0);
    put8(cards + poll_mode, 2);
    put8(at(card_poll_timer), 0x3c);
    menu_frame();
    menu_frame();
    if ((refresh_cards() & 0xffU) != 0)
        return 1;
    if (u8(u32(at(state_party)) + message_pending) != 0) {
        close_message();
        put8(u32(at(state_party)) + message_pending, 0);
    }
    return 0;
}

// 801d9e3c: leave the card mode: clear the details (801e64e0), release the
// file screen blocks (801e5b3c, 801e649c), clear every icon flag, forget
// the listings and blocks listed, then restore the CD callbacks saved by
// 801d9c84 inside a critical section.
void Overlay::leave_card_mode() {
    const auto stack_frame = enter(0x18);
    put8(at(load_state), 0);
    clear_file_title();
    release_file_blocks();
    release_details_block();
    for (std::uint32_t entry = 0; entry < 32; ++entry)
        put8(u32(at(state_card)) + entry * entry_bytes + entry_icon_shown, 0);
    put8(u32(at(state_card)) + port_files_seen, 0xff);
    put8(u32(at(state_card)) + port_files_seen + 1, 0xff);
    put32(port_blocks + 4, 0);
    put32(port_blocks, 0);
    draw_sync();
    vsync();
    auto &service = bios(*this, "EnterCriticalSection");
    static_cast<void>(service.enter_critical_section());
    auto &cd = program.resident.cd;
    cd.ready_callback = u32(saved_callbacks);    // 80040fb4
    cd.sync_callback = u32(saved_callbacks + 4); // 80040fcc
    put32(cd_read_callback, u32(saved_callbacks + 8)); // 8004373c
    service.exit_critical_section();
}

// 801e3088(step): leave a screen: by top cursor (state + 336) + `step`, 1
// and 8 leave the card mode (801d9e3c), 4 the equipment screen (801da518);
// 2, 3, 5 and 6 reach screens outside the census.
void Overlay::leave_menu_screen(std::uint32_t a0) {
    const auto stack_frame = enter(0x18);
    const auto party = [&] { return u32(at(state_party)); };
    switch (u8(at(top_cursor)) + (a0 & 0xffU)) {
    case 1:
    case 8:
        leave_card_mode();
        return;
    case 2:
        put8(party() + party_07, 0);
        put8(party() + party_08, 0);
        put8(party() + party_4b, 0);
        missing("menu_leave_screen", 0x801e3118, "symbol:menu-screen-801e2368",
                "801e2368 is outside the census");
    case 3:
        missing("menu_leave_screen", 0x801e3128, "symbol:menu-screen-801dc2cc",
                "801dc2cc is outside the census");
    case 4:
        close_equipment_shared();
        return;
    case 5:
        missing("menu_leave_screen", 0x801e3148, "symbol:menu-screen-801de36c",
                "801de36c and 801de400 are outside the census");
    case 6:
        put8(party() + party_07, 0);
        put8(party() + party_08, 0);
        put8(party() + party_4b, 0);
        missing("menu_leave_screen", 0x801e31a0, "symbol:menu-screen-801d25e4",
                "801d25e4 and 801e2b80 are outside the census");
    default:
        return;
    }
}

// ---- Gear records restored by a load (801e4d10) ---------------------------------

namespace {
std::uint32_t gear(std::uint32_t j) { return gear_records + (j & 0xffU) * record_bytes; }
} // namespace

// 801e41c0(tables, j): gear j's engine values from engine entry +2 (18h
// bytes, table +8): +60 and +64 from entry +4, +98/+9e/+9d/+9f from +14..
// +17; +60 is capped at +64.
void Overlay::recompute_gear_engine(std::uint32_t a0, std::uint32_t a1) {
    const auto record = gear(a1);
    const auto entry = u32(a0 + 8) + u8(record + 2) * 0x18;
    put32(record + 0x60, u32(entry + 4));
    put32(record + 0x64, u32(entry + 4));
    const auto maximum = u32(record + 0x64);
    put8(record + 0x98, u8(entry + 0x14));
    put8(record + 0x9e, u8(entry + 0x15));
    put8(record + 0x9d, u8(entry + 0x16));
    put8(record + 0x9f, u8(entry + 0x17));
    if (maximum < u32(record + 0x60))
        put32(record + 0x60, maximum);
}

// 801e4258(tables, j): gear j's model words +70/+72 from model entry +8
// (14h bytes, table +10) +8/+a.
void Overlay::recompute_gear_model(std::uint32_t a0, std::uint32_t a1) {
    const auto record = gear(a1);
    const auto entry = u32(a0 + 0x10) + u8(record + 8) * 0x14;
    put16(record + 0x70, u16(entry + 8));
    put16(record + 0x72, u16(entry + 0xa));
}

// 801e42ac(tables, j): gear j's frame values from frame entry +3 (10h bytes,
// table +c): +3a from +6, +3c..+3e from +c..+e and +3f from +e; +38 is
// capped at the new +3a.
void Overlay::recompute_gear_frame(std::uint32_t a0, std::uint32_t a1) {
    const auto record = gear(a1);
    const auto entry = u32(a0 + 0xc) + u8(record + 3) * 0x10;
    const auto previous = u16(record + 0x38);
    put16(record + 0x3a, u16(entry + 6));
    put8(record + 0x3c, u8(entry + 0xc));
    put8(record + 0x3d, u8(entry + 0xd));
    put8(record + 0x3e, u8(entry + 0xe));
    put8(record + 0x3f, u8(entry + 0xe));
    if (u16(record + 0x3a) < previous)
        put16(record + 0x38, u16(record + 0x3a));
}

// 801e433c(tables, j): gear j's part sums: clear the summed fields, then add
// the three part entries (+9..+b; 1ch bytes, table +14) and apply each
// part's effect kind (+15, jump table 801c524c) to the record or the
// pilot's bit words (game + 16c4 / 16da + pilot * 20, pilot 801e9808 + j);
// then the level (801e4928) and the pilot's flag 8000.
void Overlay::recompute_gear_parts(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x20);
    const auto j = a1 & 0xffU;
    const auto record = gear(j);
    const auto pilot = u8(gear_pilots + j);
    const auto bits = game + 0x16c4 + pilot * 0x20;
    const auto flags = game + 0x16da + pilot * 0x20;
    for (const auto offset : {0x40U, 0x42U, 0x44U, 0x48U})
        put16(record + offset, 0);
    for (std::uint32_t k = 0x4c; k < 0x50; ++k)
        put8(record + k, 0);
    put16(record + 0x6e, 0);
    put8(record + 0x54, 0);
    for (std::uint32_t k = 0; k < 16; ++k)
        put8(record + 0x88 + k, 0);
    for (std::uint32_t k = 0; k < 4; ++k)
        put8(record + 0x50 + k, 0);
    for (std::uint32_t k = 0; k < 3; ++k)
        put8(record + 0x55 + k, 0);
    put16(record + 0x7e, 0);
    put16(record + 0x82, 0);
    put16(record + 0x86, u16(record + 0x86) & 0xf000U);
    put16(bits, u16(bits) & 0xfb6fU);
    const auto add16 = [&](std::uint32_t offset, std::uint32_t value) {
        put16(record + offset, u16(record + offset) + value);
    };
    const auto add8 = [&](std::uint32_t offset, std::uint32_t value) {
        put8(record + offset, u8(record + offset) + value);
    };
    const auto or16 = [&](std::uint32_t address, std::uint32_t value) {
        put16(address, u16(address) | value);
    };
    const auto set_if = [&](std::uint32_t first, std::uint32_t second, std::uint32_t bit) {
        const auto word = u16(bits);
        if ((word & first) != 0 && (word & second) != 0)
            put16(bits, word | bit);
    };
    for (std::uint32_t part = 0; part < 3; ++part) {
        const auto entry = u32(a0 + 0x14) + u8(record + 9 + part) * 0x1c;
        add16(0x40, u8(entry + 0xd));
        add16(0x42, u8(entry + 0xe));
        add16(0x44, u16(entry + 6));
        add8(0x4c, u8(entry + 0x18));
        add8(0x4d, u8(entry + 0x14));
        add8(0x54, u8(entry + 0x1b));
        for (std::uint32_t k = 0; k < 4; ++k)
            add8(0x50 + k, u8(entry + 0x10 + k));
        switch (u8(entry + 0x15)) {
        case 1:
            or16(record + 0x7e, u16(entry + 0x16));
            break;
        case 2:
            or16(record + 0x82, u16(entry + 0x16));
            break;
        case 3:
            or16(record + 0x86, u16(entry + 0x16));
            break;
        case 4:
            or16(record + 0x6e, u16(entry + 0x16));
            for (std::uint32_t b = 0; b < 16; ++b)
                if ((u16(entry + 0x16) & (0x8000U >> b)) != 0)
                    add8(0x88 + b, u8(entry + 0x1a));
            break;
        case 5:
            add8(0x4f, u8(entry + 0x16));
            break;
        case 6:
            set_if(0x1000, 0x800, 0x400);
            break;
        case 7:
            set_if(0x200, 0x100, 0x80);
            break;
        case 8:
            set_if(0x40, 0x20, 0x10);
            break;
        case 9: // continues into case 10 (801e4684)
            or16(record + 0x48, u16(entry + 0x16));
            [[fallthrough]];
        case 10:
            add8(0x56, u8(entry + 0x16));
            break;
        case 11:
            add8(0x57, u8(entry + 0x16));
            break;
        default:
            break;
        }
    }
    put8(record + 0x4a, gear_part_level(j));
    if (u8(record + 0x4f) != 0)
        put16(flags, u16(flags) | 0x8000U);
    else if (j == u8(character_record(u8(gear_pilots + j)) + 0xa0))
        put16(flags, u16(flags) & 0x7fffU);
}

// 801e4928(j): gear j's level: ((+44) / 120 - +75) / 2 (C division), 0 when
// the low halfword is negative, as a byte.
std::uint32_t Overlay::gear_part_level(std::uint32_t a0) {
    const auto record = gear(a0);
    const auto difference =
        static_cast<std::int32_t>((u16(record + 0x44) / 120) & 0xffffU) -
        static_cast<std::int32_t>(u8(record + 0x75));
    const auto half = difference / 2;
    const auto level = static_cast<std::int16_t>(half & 0xffff) < 0 ? 0 : half;
    return static_cast<std::uint32_t>(level) & 0xffU;
}

// 801e4a28(payload): copy the game data into the payload (store_game_data).
void Overlay::store_payload_game_data(std::uint32_t a0) {
    const auto stack_frame = enter(0x18);
    Menu context{*program.menu, program.resident.sound};
    store_game_data(context, a0);
}

// 801e4d10(payload, tables): restore the game data from a payload
// (restore_game_data: characters, pilot words, gears through 801e41c0,
// 801e4258, 801e42ac and 801e433c, the other ranges).
void Overlay::restore_payload_game_data(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x30);
    Menu context{*program.menu, program.resident.sound};
    restore_game_data(context, a0, a1);
}

// ---- File details panel --------------------------------------------------------

// 801e5b3c: release the file screen's 32 blocks (state + 3a8..).
void Overlay::release_file_blocks() {
    const auto stack_frame = enter(0x18);
    for (std::uint32_t i = 0; i < 32; ++i)
        release(u32(at(file_blocks + i * 4)), 0x801e5b60);
}

// 801e61b0: the details panel's fixed labels, per party column (87ch bytes
// of the details block from +0): up to nine label sprites (ids 801ea494,
// ffff none, at 801e9f98 + 50h * column, 801e9fbc) counted at +1312, the
// build buffer at +130e, and the column's name quad (+a98 + 820 + buffer *
// 28h: 801e927c, texture page (0, 0, 180h, 0), the even-row text CLUT,
// 801e920c at the name image position) with the buffer at +1311.
void Overlay::draw_details_labels() {
    const auto stack_frame = enter(0x40);
    for (std::uint32_t column = 0; column < 3; ++column) {
        const auto base = column * 0x87c;
        const auto block = [&] { return u32(at(file_details)); };
        put8(block() + base + 0x1312, 0);
        for (std::uint32_t n = 0; n < 9; ++n) {
            const auto id = u32(label_ids + n * 4);
            if (id == 0xffff)
                continue;
            const auto count = u8(block() + base + 0x1312);
            const auto parts = resident::sheet_quads(
                *this, u32(at(sprite_sheet)), id, block() + base + 0xa98 + count * 0x50 + 0x50,
                u32(at(buffer_index)), u32(label_x + n * 4) + column * 0x50,
                u32(label_y + n * 4), 0x1000);
            put8(block() + base + 0x1312, u8(block() + base + 0x1312) + parts);
        }
        put8(block() + base + 0x130e, u8(at(buffer_index)));
        const auto quad = [&] { return block() + base + 0xa98 + 0x820 + u32(at(buffer_index)) * 0x28; };
        init_text_quad(quad());
        put16(quad() + 0x16, get_tpage(0, 0, 0x180, 0));
        put16(quad() + 0x0e, u16(0x800595d4));
        set_quad_rect(quad(), (u16(label_x) + column * 0x50) & 0xffffU, (u16(label_y) + 7) & 0xffffU,
                  (u32(name_image_x + column * 4) << 2) & 0xfcU, u8(name_image_y + column * 4),
                  0x48, 0xd);
        put8(block() + base + 0x1311, u8(at(buffer_index)));
    }
}

// 801e649c: release the details block (state + 34c) and clear the details
// flag.
void Overlay::release_details_block() {
    const auto stack_frame = enter(0x18);
    release(u32(at(file_details)), 0x801e64b0);
    put8(u32(at(state_party)) + file_details_shown, 0);
}

// 801e64e0: clear the title image area (140h, e0h, 40h x 20h) and the
// details flag.
void Overlay::clear_file_title() {
    const auto stack_frame = enter(0x20);
    auto f = frame(0x20);
    const auto rect = f[0x10];
    put16(rect, 0x140);
    put16(rect + 2, 0xe0);
    put16(rect + 4, 0x40);
    put16(rect + 6, 0x20);
    program.clear_image(services, rect, 0); // 80044764 ClearImage(rect, 0, 0, 0)
    put8(u32(at(state_party)) + file_details_shown, 0);
}

// 801e6544(pixels): narrow each 16-pixel row of a 16-row glyph to 12: each
// group of four bytes a, b, c, d becomes a, b | c, d.
void Overlay::narrow_glyph_rows(std::uint32_t a0) {
    for (std::uint32_t row = 0; row < 16; ++row) {
        const auto at_row = a0 + row * 16;
        std::array<std::uint32_t, 16> in{};
        for (std::uint32_t i = 0; i < 16; ++i)
            in[i] = u8(at_row + i);
        for (std::uint32_t group = 0; group < 4; ++group) {
            put8(at_row + group * 3, in[group * 4]);
            put8(at_row + group * 3 + 1, in[group * 4 + 1] | in[group * 4 + 2]);
            put8(at_row + group * 3 + 2, in[group * 4 + 3]);
        }
    }
}

// 801e65e4(text): the ROM glyph (Krom2RawAdd 800405c4) of the character at
// `text`: a two-byte Shift-JIS character as is (801ea8c0 = 1), an ASCII byte
// through 801ea5d0 or, below 20h, the full-width space 8140 (801ea8c0 = 0).
std::uint32_t Overlay::kanji_glyph_address(std::uint32_t a0) {
    const auto stack_frame = enter(0x18);
    auto high = u8(a0);
    auto low = u8(a0 + 1);
    put8(two_byte_character, 1);
    if (high < 0x80) {
        if (high < 0x20) {
            high = 0x81;
            low = 0x40;
        } else {
            const auto code = ascii_codes + high * 2;
            low = u8(code);
            high = u16(code) >> 8;
        }
        put8(two_byte_character, 0);
    }
    return bios(*this, "Krom2RawAdd").kanji_address(low | (high << 8));
}

// 801e6668(file): the file's title (head + 4, up to 32 characters or 64
// bytes) drawn from the BIOS font: each glyph's 16 rows (ROM halfwords, low
// byte's bits then high byte's, most significant first) as one byte per
// pixel, narrowed to 12 columns (801e6544) and packed as 4-bit pixels, 16
// glyphs a row, into a 1000h-byte image loaded at (140h, e0h, 40h x 20h).
void Overlay::draw_file_title(std::uint32_t a0) {
    const auto stack_frame = enter(0x38);
    auto f = frame(0x38);
    const auto rect = f[0x10];
    const auto pixels = allocate(0x100, 1, 0x801e6690);
    const auto image = allocate(0x1000, 1, 0x801e66a0);
    bzero(image, 0x1000);
    std::uint32_t used = 0; // S3: bytes of the title consumed
    auto text = u32(at(state_card)) + (a0 << 9) + heads + 4;
    for (std::uint32_t glyph = 0; u8(text) != 0;) {
        const auto rom = kanji_glyph_address(text);
        if (rom != none) {
            auto out = pixels;
            auto &service = bios(*this, "ROM read");
            for (std::uint32_t row = 0; row < 16; ++row) {
                const auto bits = service.rom_halfword(rom + row * 2);
                for (std::int32_t bit = 7; bit >= 0; --bit)
                    put8(out++, (bits >> static_cast<std::uint32_t>(bit)) & 1U);
                for (std::int32_t bit = 15; bit >= 8; --bit)
                    put8(out++, (bits >> static_cast<std::uint32_t>(bit)) & 1U);
            }
            narrow_glyph_rows(pixels);
            for (std::uint32_t row = 0; row < 16; ++row)
                for (std::uint32_t column = 0; column < 12; ++column) {
                    const auto word = image + 2 * ((glyph / 16) * 1024 + (glyph % 16) * 4 +
                                                   row * 64 + column / 4);
                    put16(word, u16(word) | (u8(pixels + row * 16 + column) << ((column % 4) * 4)));
                }
        }
        ++text;
        if (u8(two_byte_character) != 0) {
            ++text;
            ++used;
        }
        ++used;
        if (used >= 64 || ++glyph >= 32)
            break;
    }
    put16(rect, 0x140);
    put16(rect + 2, 0xe0);
    put16(rect + 4, 0x40);
    put16(rect + 6, 0x20);
    load_image(rect, image);
    draw_sync();
    release(pixels, 0x801e6874);
    release(image, 0x801e687c);
}

// 801e68ac(payload): the details' play time and file number sprites: two
// colons (sprite ee at 801e9fe0/e4, y 7a), the play counter split
// (801c7f34) into seven digit sprites (801e9fe8..), the labels 17 and 32 and
// the file number (payload + 23 + 1) as two digits.
void Overlay::draw_details_time(std::uint32_t a0) {
    const auto stack_frame = enter(0x40);
    const auto block = [&] { return u32(at(file_details)); };
    const auto quads = [&](std::uint32_t id, std::uint32_t packets, std::uint32_t x,
                           std::uint32_t y) {
        return resident::sheet_quads(*this, u32(at(sprite_sheet)), id, block() + packets,
                                     u32(at(buffer_index)), x, y, 0x1000);
    };
    static_cast<void>(quads(0xee, 0x240c, u32(time_x), 0x7a));
    static_cast<void>(quads(0xee, 0x24ac, u32(time_x + 4), 0x7a));
    split_play_time(u32(a0));
    for (std::uint32_t i = 0; i < 7; ++i)
        static_cast<void>(quads(u32(at(0x2ec + i * 4)), 0x254c + i * 0x50,
                                u32(time_x + 8 + i * 4), 0x7a));
    static_cast<void>(quads(0x17, 0x2c7c, 8, 0x66));
    static_cast<void>(quads(0x32, 0x2ccc, 0x10, 0x66));
    const auto number = u8(a0 + 0x23) + 1;
    static_cast<void>(quads(signed_div(number, 10), 0x2d1c, 0x10, 0x6e));
    const auto again = u8(a0 + 0x23) + 1;
    static_cast<void>(quads(again - signed_div(again, 10) * 10, 0x2d6c, 0x18, 0x6e));
}

// 801e6ae8(column, payload): the column's character portrait (sprite 14eh
// + party id) at 801ea004/801ea010.
void Overlay::draw_details_portrait(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x28);
    const auto column = a0 & 0xffU;
    static_cast<void>(resident::sheet_quads(
        *this, u32(at(sprite_sheet)), u8(a1 + column + 0x1c) + 0x14e,
        u32(at(file_details)) + 0xa98 + column * 0x87c, u32(at(buffer_index)),
        u32(member_x + column * 4), u32(member_y + column * 4), 0x1000));
}

namespace {
// The digit sprites of menu state + 322 (+ first) as 801e6b70.. draw them:
// each not ff digit at x + 8 * (index or drawn count) into the column's
// packets from `packets`, counting parts at `counter`.
void draw_digits(Overlay &overlay, std::uint32_t column, std::uint32_t first,
                 std::uint32_t digits, std::uint32_t counter, std::uint32_t packets,
                 std::uint32_t x, std::uint32_t y, bool by_drawn) {
    const auto base = column * 0x87c;
    const auto block = [&] { return overlay.u32(overlay.at(file_details)); };
    overlay.put8(block() + base + counter, 0);
    std::uint32_t drawn = 0;
    for (std::uint32_t i = 0; i < digits; ++i) {
        const auto digit = overlay.u8(overlay.at(number_digits + first + i));
        if (digit == 0xff)
            continue;
        const auto count = overlay.u8(block() + base + counter);
        const auto parts = resident::sheet_quads(
            overlay, overlay.u32(overlay.at(sprite_sheet)), digit,
            block() + base + 0xa98 + count * 0x50 + packets, overlay.u32(overlay.at(buffer_index)),
            column * 0x50 + overlay.u32(x) + (by_drawn ? drawn : i) * 8, overlay.u32(y), 0x1000);
        overlay.put8(block() + base + counter, overlay.u8(block() + base + counter) + parts);
        ++drawn;
    }
}
} // namespace

// 801e6b70(column, payload): the column's level digits (payload + 16 +
// column through 801c80b8) counted at +1308; the second value (payload + 19)
// is split but only its counter +1309 is cleared.
void Overlay::draw_details_level(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x38);
    const auto column = a0 & 0xffU;
    split_decimal_digits(u8(a1 + column + 0x16));
    draw_digits(*this, column, 0, 3, 0x1308, 0x320, level_x, level_y, false);
    split_decimal_digits(u8(a1 + (a0 & 0xffU) + 0x19));
    put8(u32(at(file_details)) + (a0 & 0xffU) * 0x87c + 0x1309, 0);
}

// 801e6cfc(column, payload): the column's HP (payload + 4 + 2 * column) and
// maximum HP (+a) digits, counted at +130a and +130b.
void Overlay::draw_details_hp(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x40);
    const auto column = a0 & 0xffU;
    split_decimal_digits(u16(a1 + column * 2 + 4));
    draw_digits(*this, column, 0, 3, 0x130a, 0x500, hp_x, hp_y, false);
    split_decimal_digits(u16(a1 + column * 2 + 0xa));
    draw_digits(*this, column, 0, 3, 0x130b, 0x5f0, max_hp_x, max_hp_y, true);
}

// 801e6f5c(column, payload): two-digit values payload + 10 and + 13 of the
// column (digits 323, 324), counted at +130c and +130d.
void Overlay::draw_details_values(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x40);
    const auto column = a0 & 0xffU;
    split_decimal_digits(u8(a1 + column + 0x10));
    draw_digits(*this, column, 1, 2, 0x130c, 0x6e0, stat_x, stat_y, false);
    split_decimal_digits(u8(a1 + column + 0x13));
    draw_digits(*this, column, 1, 2, 0x130d, 0x780, stat2_x, stat2_y, true);
}

// 801e71b4(column, payload, file): the column's character name from the
// file head's payload copy (names from head + 100 + 24, encoded) decoded
// (80033b34), laid out (80034eac, width 24h, plane 0) in a 3f6h-byte image
// and loaded at (801ea590 + 180h, 801ea5dc; 28h x 0dh).
void Overlay::draw_details_name(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2) {
    const auto stack_frame = enter(0x58);
    auto f = frame(0x58);
    const auto rect = f[0x10];
    const auto codes = f[0x18];
    const auto text = f[0x30];
    const auto column = a0 & 0xffU;
    const auto payload = u32(at(state_card)) + (a2 << 9) + heads + 0x100;
    std::uint32_t at_byte = 0;
    for (; at_byte < name_size; at_byte += 2) {
        const auto name = payload + u8(a1 + column + 0x1c) * name_size + 0x24;
        put8(codes + at_byte, u8(name + at_byte));
        put8(codes + at_byte + 1, u8(name + at_byte + 1));
        if (u8(codes + at_byte) == 0 && u8(codes + at_byte + 1) == 0)
            break;
    }
    decode_codes(*this, codes, text, at_byte / 2);
    const auto image = allocate(0x3f6, 0, 0x801e729c);
    bzero(image, 0x3f6);
    const auto glyphs = [&](std::uint32_t window) { program.dialogue_glyphs(window); };
    static_cast<void>(resident::layout_text_line(*this, text, image, 0x24, 0, glyphs));
    put16(rect, u16(name_image_x + column * 4) + 0x180);
    put16(rect + 4, 0x28);
    put16(rect + 6, 0xd);
    put16(rect + 2, u16(name_image_y + column * 4));
    load_image(rect, image);
    draw_sync();
    release(image, 0x801e7318);
}

// 801e733c: the title strip: 16 textured quads per buffer (details block +
// 277c + (2 * n + buffer) * 28h; 801e927c) of 12 x 16 pixels at 801ea04c +
// 12 * n, 801ea050, texture (16 * n, f0h) .. (16 * n + 12, ffh) of page
// (0, 0, 140h, 80h) and CLUT (0, 1c0h).
void Overlay::build_title_strip() {
    const auto stack_frame = enter(0x30);
    for (std::uint32_t n = 0; n < 16; ++n) {
        const auto quad = [&] {
            return u32(at(file_details)) + 0x277c + (n * 2 + u32(at(buffer_index))) * 0x28;
        };
        init_text_quad(quad());
        const auto x = u16(title_x) + n * 12;
        const auto y = u16(title_y);
        put16(quad() + 0x08, x);
        put16(quad() + 0x0a, y);
        put16(quad() + 0x10, x + 12);
        put16(quad() + 0x12, y);
        put16(quad() + 0x18, x);
        put16(quad() + 0x1a, y + 16);
        put16(quad() + 0x20, x + 12);
        put16(quad() + 0x22, y + 16);
        put8(quad() + 0x0c, n * 16);
        put8(quad() + 0x0d, 0xf0);
        put8(quad() + 0x14, n * 16 + 12);
        put8(quad() + 0x15, 0xf0);
        put8(quad() + 0x1c, n * 16);
        put8(quad() + 0x1d, 0xff);
        put8(quad() + 0x24, n * 16 + 12);
        put8(quad() + 0x25, 0xff);
        put16(quad() + 0x16, get_tpage(0, 0, 0x140, 0x80));
        put16(quad() + 0x0e, get_clut(0, 0x1c0));
    }
}

// 801e76ec(file): the details panel of a file: fixed labels (801e61b0),
// then per party column of its payload head (+1c member, ff empty: +1310
// flag) the portrait, level, HP, values and name (801e6ae8 .. 801e71b4) and
// the buffer at +130f; then the time and number (801e68ac) and the title
// strip (801e733c).
void Overlay::draw_file_details(std::uint32_t a0) {
    const auto stack_frame = enter(0x28);
    const auto payload = u32(at(state_card)) + (a0 << 9) + heads + 0x100;
    draw_details_labels();
    for (std::uint32_t column = 0; column < 3; ++column) {
        const auto base = column * 0x87c;
        if (u8(payload + column + 0x1c) != 0xff) {
            put8(u32(at(file_details)) + base + 0x1310, 1);
            draw_details_portrait(column, payload);
            draw_details_level(column, payload);
            draw_details_hp(column, payload);
            draw_details_values(column, payload);
            draw_details_name(column, payload, a0);
        } else {
            put8(u32(at(file_details)) + base + 0x1310, 0);
        }
        put8(u32(at(file_details)) + base + 0x130f, u8(at(buffer_index)));
    }
    draw_details_time(payload);
    build_title_strip();
}

// 801e781c(file, game): show file `file`'s details: clear the title image
// (801e64e0); none for ff; this game's file gets the panel (801e76ec) and
// its title (801e6668) with +2dbc set; others only the title (+2dbc 0).
void Overlay::show_file_details(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x20);
    clear_file_title();
    if (a0 == 0xff)
        return;
    if ((a1 & 0xffU) != 0) {
        draw_file_details(a0);
        draw_file_title(a0);
        put8(u32(at(file_details)) + 0x2dbc, 1);
    } else {
        draw_file_title(a0);
        put8(u32(at(file_details)) + 0x2dbc, 0);
    }
    put8(u32(at(state_party)) + file_details_shown, 1);
}

// 801e78c8(entry): load a file's icon: its palette (head + 60, 20h bytes,
// copied to 801ea8c4) to CLUT (16 * entry, entry / 16 + 1c1h), its three
// frames (head + 80 + 80h * frame) to VRAM (140h + 4 * (entry % 16), 80h +
// 16 * port + 32 * frame; 4 x 16); the entry's frame rows by the icon flag
// (head + 2: 11h one frame, 12h two, 13h three; others clear the entry's
// icon flag); the port's blocks listed grow by the file's block count.
void Overlay::load_file_icon(std::uint32_t a0) {
    const auto stack_frame = enter(0x38);
    const auto entry = a0;
    const auto port = signed_div(entry, 16);
    const auto head = [&] { return u32(at(state_card)) + (entry << 9) + heads; };
    put16(icon_rect, entry * 4 - ((port << 6) - 0x140));
    put16(icon_rect + 4, 4);
    put16(icon_rect + 6, 0x10);
    put16(palette_rect, entry << 4);
    put16(palette_rect + 2, port + 0x1c1);
    put16(palette_rect + 4, 0x10);
    put16(palette_rect + 6, 1);
    static_cast<void>(memmove(icon_palette, head() + 0x60, 0x20));
    load_image(palette_rect, icon_palette);
    draw_sync();
    const auto row = port << 4;
    const auto top = row + 0x80;
    for (std::uint32_t frame_index = 0; frame_index < 3; ++frame_index) {
        put16(icon_rect + 2, frame_index * 32 + top);
        load_image(icon_rect, head() + 0x80 + frame_index * 0x80);
        draw_sync();
    }
    // The entry's six icon frame rows (each frame shown twice per cycle).
    const auto second = row + 0xa0;
    const auto third = row + 0xc0;
    std::optional<std::array<std::uint32_t, 6>> rows;
    switch (u8(head() + 2)) {
    case 0x11:
        rows = {top, top, top, top, top, top};
        break;
    case 0x12:
        rows = {top, second, top, second, top, second};
        break;
    case 0x13:
        rows = {top, second, third, top, second, third};
        break;
    default:
        break;
    }
    if (rows)
        for (std::uint32_t i = 0; i < rows->size(); ++i)
            put32(u32(at(state_card)) + entry * entry_bytes + i * 4, (*rows)[i]);
    put32(port_blocks + port * 4, u32(port_blocks + port * 4) + u8(head() + 3));
    if (!rows)
        put8(u32(at(state_card)) + entry * entry_bytes + entry_icon_shown, 0);
}

} // namespace xem::reconstruction::menu
