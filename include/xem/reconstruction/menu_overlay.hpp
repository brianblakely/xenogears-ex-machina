#pragma once

#include "xem/reconstruction/program.hpp"
#include "xem/reconstruction/resident_memory.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <span>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// The main menu overlay (sha256 82f84a24..., loaded at 801c5000) as one
// connected program: the menu mode's entry 801c62a8, its frame 801c7bf4, the
// title, field-menu, item, equipment and file screens, their windows and the
// memory-card management around the save and load decisions.
//
// The overlay runs on menu-mode memory at original addresses: the heap blocks
// it and the resident mode setup allocated (menu::MenuMemory regions, each
// block owned whole), the persistent game data (moved into those regions for
// the duration of an Overlay), resident globals through the Program's original
// layout, and callee stack frames below the entry stack pointer. Resident
// services (heap, libgpu, GTE, pad queue, sound driver, disc reads) are the
// Program's reconstructed operations. Memory-card BIOS calls are explicit
// services (CardBios). Every unrecovered path throws MissingDependency.
namespace xem::reconstruction::menu {

// Menu-card BIOS calls: the kernel and memory-card entries the menu reaches
// through the resident stubs (A0/B0 table jumps and syscalls at 80040464..
// 800405c4, 8004e784) and the libcard entries 8004e794/8004e7e8. The
// contracts are the general PS1 BIOS and library ones (general
// documentation, not game facts). Values are the original registers: V0 as
// a word (-1 is ffffffff); names are the C strings the menu passes. Memory a
// call fills is returned by the service; the Overlay stores it at the
// original address. With no CardBios attached (Overlay::card null) every
// call throws ServiceUnavailable. menu_card.cpp makes the calls.
class CardBios {
  public:
    virtual ~CardBios() = default;

    // A directory entry (BIOS DIRENTRY): name[20], attr, size, next, head,
    // system[4]: 40 bytes.
    using DirectoryEntry = std::array<std::uint8_t, 40>;
    struct ReadResult {
        std::uint32_t result{};          // V0: bytes read, or -1
        std::vector<std::uint8_t> bytes; // what the call stores at the buffer
    };

    // 80040464 A(70h) _bu_init(): initialize the memory-card file system.
    virtual void bu_init() = 0;
    // 80040474 B(08h) OpenEvent(class, spec, mode, handler): V0 the event
    // descriptor (-1 when none is free).
    virtual std::uint32_t open_event(std::uint32_t event_class, std::uint32_t spec,
                                     std::uint32_t mode, std::uint32_t handler) = 0;
    // 80040484 B(09h) CloseEvent(event): V0 1.
    virtual std::uint32_t close_event(std::uint32_t event) = 0;
    // 80040494 B(0Bh) TestEvent(event): V0 1 when the event was delivered
    // (its state returns to enabled), else 0.
    virtual std::uint32_t test_event(std::uint32_t event) = 0;
    // 800404a4 B(0Ch) EnableEvent(event): V0 1.
    virtual std::uint32_t enable_event(std::uint32_t event) = 0;
    // 800404c4 B(20h) UnDeliverEvent(class, spec): pending deliveries of the
    // event return to enabled.
    virtual void undeliver_event(std::uint32_t event_class, std::uint32_t spec) = 0;
    // 800404d4 syscall(1) EnterCriticalSection(): V0 1 when interrupts were
    // enabled. 800404e4 syscall(2) ExitCriticalSection().
    virtual std::uint32_t enter_critical_section() = 0;
    virtual void exit_critical_section() = 0;
    // 80040534 B(32h) open(name, mode): V0 the file descriptor, -1 on
    // failure. Mode 1 read, 2 write, 3 both; 200h create with the block count
    // in bits 16-31.
    virtual std::uint32_t open(std::string_view name, std::uint32_t mode) = 0;
    // 80040544 B(34h) read(fd, buffer, count): V0 the bytes read or -1; the
    // bytes read are stored at the buffer (at most `count`).
    virtual ReadResult read(std::uint32_t fd, std::uint32_t count) = 0;
    // 80040554 B(35h) write(fd, buffer, count): V0 the bytes written or -1.
    virtual std::uint32_t write(std::uint32_t fd, std::span<const std::uint8_t> bytes) = 0;
    // 80040564 B(36h) close(fd): V0 fd, or -1.
    virtual std::uint32_t close(std::uint32_t fd) = 0;
    // 80040574 B(41h) format(device): V0 1 on success, 0 on failure.
    virtual std::uint32_t format(std::string_view device) = 0;
    // 80040584 B(42h) firstfile(pattern, dir) and 80040594 B(43h)
    // nextfile(dir): the first / next directory entry matching the pattern
    // (V0 is `dir`, the entry stored there) or none (V0 0).
    virtual std::optional<DirectoryEntry> first_file(std::string_view pattern) = 0;
    virtual std::optional<DirectoryEntry> next_file() = 0;
    // 800405a4 B(44h) rename(old, new) and 800405b4 B(45h) erase(name): V0
    // 1 on success, 0 on failure.
    virtual std::uint32_t rename(std::string_view from, std::string_view to) = 0;
    virtual std::uint32_t erase(std::string_view name) = 0;
    // 800405c4 B(51h) Krom2RawAdd(code): V0 the BIOS ROM address of the
    // 16x15 Kanji-font glyph of Shift-JIS `code` (one halfword per row), -1
    // when the font has none. The menu then reads the glyph rows from ROM:
    // rom_halfword(address) is that read.
    virtual std::uint32_t kanji_address(std::uint32_t code) = 0;
    virtual std::uint32_t rom_halfword(std::uint32_t address) = 0;
    // 8004e784 A(ABh) _card_info(port): V0 1 when the request was accepted
    // (the outcome is delivered as a card event), 0 otherwise. Port 0 or 10h.
    virtual std::uint32_t card_info(std::uint32_t port) = 0;
    // 8004e794 InitCARD(pad_enable) (libcard: ChangeClearPAD(0), B(4Ah)
    // InitCARD2, the kernel card patches) and 8004e7e8 StartCARD() (B(4Bh)
    // StartCARD2, ChangeClearPAD(0)).
    virtual void init_card(std::uint32_t pad_enable) = 0;
    virtual void start_card() = 0;
};

class Overlay : public resident::Memory {
  public:
    // A menu computation over `program` (which must hold menu memory). The
    // game data moves into menu memory until the Overlay is destroyed.
    // `stack_pointer` is SP at the entry the host invokes; callee frames lie
    // below it. `services` supplies platform results of libgpu and VSync
    // calls; `card` the card BIOS (null when the path makes no card call).
    Overlay(Program &program, FrameServices &services, std::uint32_t stack_pointer,
            CardBios *card = nullptr);
    ~Overlay() override;
    Overlay(const Overlay &) = delete;
    Overlay &operator=(const Overlay &) = delete;

    Program &program;
    FrameServices &services;
    CardBios *card;
    // Optional observation of completed frames: called at each menu frame's
    // entry (801c7bf4) once the interrupts that preceded it are delivered.
    // Read-only; it supplies nothing.
    std::function<void(std::string_view boundary)> boundary;

    // Interrupt arrivals (platform inputs) are ordered among the overlay's
    // own events: each frame entry (801c7bf4), each frame's VSync(0) return
    // and each platform service result the overlay consumes, counted from
    // the Overlay's construction. An arrival's platform site is the number of
    // those events that preceded it; it is delivered at the first point the
    // overlay has passed that many (before each service-consuming call and
    // at the frame positions). Waits deliver arrivals themselves.
    void catch_up();
    void pass_position();
    // With `sound_positions` each entry of the menu sound 801c8574 is an
    // event (a capture that hooks it); otherwise the sound only catches up.
    bool sound_positions = false;
    void entering_sound();
    // Events passed so far (positions and consumed service results).
    [[nodiscard]] std::uint32_t events() const;

    // Run the overlay function whose entry is `address` with argument
    // registers `arguments` (A0..A3, then the caller's stack words); returns
    // V0 (zero for a function without a result). Unknown entries throw
    // MissingDependency.
    std::uint32_t call(std::uint32_t address, const std::vector<std::uint32_t> &arguments);

    // ---- Memory at original addresses -------------------------------------
    // resident::Memory: menu memory, callee frames, then Program-owned memory.
    [[nodiscard]] std::uint32_t read(std::uint32_t address, std::uint32_t width) const override;
    void write(std::uint32_t address, std::uint32_t value, std::uint32_t width) override;
    [[nodiscard]] std::uint32_t u8(std::uint32_t address) const;
    [[nodiscard]] std::uint32_t u16(std::uint32_t address) const;
    [[nodiscard]] std::uint32_t u32(std::uint32_t address) const;
    [[nodiscard]] std::int32_t s8(std::uint32_t address) const;
    [[nodiscard]] std::int32_t s16(std::uint32_t address) const;
    [[nodiscard]] std::int32_t s32(std::uint32_t address) const { return static_cast<std::int32_t>(u32(address)); }
    void put8(std::uint32_t address, std::uint32_t value);
    void put16(std::uint32_t address, std::uint32_t value);
    void put32(std::uint32_t address, std::uint32_t value);

    // The menu state block (*800625a0) and a field of it.
    [[nodiscard]] std::uint32_t state() const { return u32(state_pointer); }
    [[nodiscard]] std::uint32_t at(std::uint32_t offset) const { return state() + offset; }

    // The stack below the entry stack pointer (stack_bytes of it) is memory
    // like any other: it keeps what earlier callees left there, starting from
    // `image` (the entry image's bytes below SP, lowest address first; zeros
    // when the host has none). The comparison excludes it; stale contents
    // matter only where the original reads bytes it did not write.
    static constexpr std::uint32_t stack_bytes = 0x4000;
    void set_stack_image(std::span<const std::uint8_t> image);
    // Each overlay function enters its original stack frame (the size its
    // prologue subtracts from SP) for its duration, so that locals whose
    // address the original passes on lie where the original kept them.
    class StackFrame {
      public:
        StackFrame(Overlay &overlay, std::uint32_t bytes);
        ~StackFrame();
        StackFrame(const StackFrame &) = delete;
        StackFrame &operator=(const StackFrame &) = delete;

      private:
        Overlay &overlay_;
        std::uint32_t saved_sp_;
        std::uint32_t saved_size_;
    };
    [[nodiscard]] StackFrame enter(std::uint32_t bytes) { return StackFrame(*this, bytes); }
    // The current function's frame: `frame(bytes)[offset]` is SP + offset
    // as the original addresses its locals. `bytes` (the extent used) must
    // lie within the entered frame.
    struct Frame {
        std::uint32_t base;
        [[nodiscard]] std::uint32_t operator[](std::uint32_t offset) const { return base + offset; }
    };
    [[nodiscard]] Frame frame(std::uint32_t bytes) const;

    // An unrecovered path: MissingDependency naming the original address.
    [[noreturn]] static void missing(std::string_view operation, std::uint32_t address,
                                     const char *id, const char *reason);

    // ---- Resident services -----------------------------------------------
    // 80031bdc(size, mode) from the JAL at `site`: a new menu-owned block.
    std::uint32_t allocate(std::uint32_t size, std::uint32_t mode, std::uint32_t site);
    // 800320e8(block) from the JAL at `site`.
    void release(std::uint32_t block, std::uint32_t site);
    // C library (resident 8003f8e8 bzero, 8003f968 memcpy, 8003f99c memmove,
    // 8003fb84 strcpy, 8003fa78 strcat, 8003fbc8 strlen). Each returns V0.
    std::uint32_t bzero(std::uint32_t address, std::int32_t count);
    std::uint32_t memcpy(std::uint32_t to, std::uint32_t from, std::int32_t count);
    std::uint32_t memmove(std::uint32_t to, std::uint32_t from, std::int32_t count);
    std::uint32_t strcpy(std::uint32_t to, std::uint32_t from);
    std::uint32_t strcat(std::uint32_t to, std::uint32_t from);
    [[nodiscard]] std::uint32_t strlen(std::uint32_t text) const;
    // 8001bd40: random byte in lo..hi from rand (8003fa38).
    std::uint32_t random_range(std::uint32_t low, std::uint32_t high);

    // libgpu packet helpers (80043a1c GetTPage, 80043a58 GetClut, 80043b48
    // AddPrim, 80043bfc SetSemiTrans, 80043c24 SetShadeTex, 80043c9c
    // SetPolyF4, 80043cb0 SetPolyFT4, 80043cc4 SetPolyG4, 80043da0 SetLineF3).
    [[nodiscard]] static std::uint32_t get_tpage(std::uint32_t tp, std::uint32_t abr,
                                                 std::int32_t x, std::int32_t y);
    [[nodiscard]] static std::uint32_t get_clut(std::int32_t x, std::int32_t y);
    void add_prim(std::uint32_t table_entry, std::uint32_t packet);
    void set_semi_trans(std::uint32_t packet, std::uint32_t on);
    void set_shade_tex(std::uint32_t packet, std::uint32_t on);
    void set_poly_f4(std::uint32_t packet);
    void set_poly_ft4(std::uint32_t packet);
    void set_poly_g4(std::uint32_t packet);
    void set_line_f3(std::uint32_t packet);
    // libgpu calls through the request queue, with platform results from
    // `services`: 80044894 LoadImage (rect at `rect`), 800445d0 DrawSync(0),
    // 8004b54c VSync(0), 80044c44 PutDrawEnv, 80044e9c PutDispEnv,
    // 8004495c MoveImage, 80044bd0 DrawOTag, 80044ad8 ClearOTagR.
    void load_image(std::uint32_t rect, std::uint32_t data);
    void draw_sync();
    void vsync();
    void put_draw_env(std::uint32_t environment);
    void put_disp_env(std::uint32_t environment);
    void move_image(std::uint32_t rect, std::int32_t x, std::int32_t y);
    void draw_otag(std::uint32_t table);
    void clear_otag_r(std::uint32_t table, std::uint32_t count);

    // 800454dc SetDrawMode(packet, dfe, dtd, tpage, window): the window
    // rectangle at `window` (zero for none).
    void set_draw_mode(std::uint32_t packet, std::uint32_t dfe, std::uint32_t dtd,
                       std::uint32_t tpage, std::uint32_t window);
    // 800471b4 OpenTIM(data) and 800471c4 ReadTIM(image): the next TIM of the
    // opened data as a TIM_IMAGE record at `image` (mode, CLUT rectangle and
    // words, image rectangle and words); returns `image`, or 0 at the end.
    void open_tim(std::uint32_t data);
    std::uint32_t read_tim(std::uint32_t image);
    // Disc files (resident 80028470 select, 800288ec rounded size, 800295d8
    // read, 80028a60 wait, 80028530 current disc).
    std::int32_t select_directory(std::uint32_t base, std::uint32_t index);
    [[nodiscard]] std::uint32_t file_words(std::uint32_t file);
    std::int32_t read_file(std::uint32_t file, std::uint32_t destination, std::uint32_t offset,
                           std::uint32_t mode);
    void disc_wait(std::uint32_t once);
    [[nodiscard]] std::uint32_t current_disc() const;
    // 80033698(x, y): load the text palette row at (x, y) and keep both text
    // CLUT ids (800595d4, 80059414).
    void load_text_palette(std::uint32_t x, std::uint32_t y);

    // Resident text: 80034eac lays `text` out into the glyph image at
    // `image` (resident::layout_text_line with the glyph renderer 80033df0);
    // 80033b34 turns `count` name codes at `codes` into their bytes at `text`
    // (menu::decode_text with the loaded code table).
    std::uint32_t layout_text(std::uint32_t text, std::uint32_t image, std::uint32_t width,
                              std::uint32_t plane);
    void decode_text(std::uint32_t codes, std::uint32_t text, std::uint32_t count);

    // Sound: 801c8574 (a menu effect when the menu plays sounds).
    void play_sound(std::uint32_t id);
    // 80035734(port): the controller kind from the pad receive buffer (port
    // buffers 800625fc, 8006261e): 0 when the receive status is ff, else by
    // the high nibble of the type byte: 4x 1, 1x 2, 5x 3, 7x 4, others -1.
    [[nodiscard]] std::uint32_t pad_kind(std::uint32_t port) const;

    // ---- Overlay functions -----------------------------------------------
    // Every census function of the image, by entry address. Arguments and
    // results are the original registers (A0..A3 then stack words; V0).
    // Names follow each function's role; the leading address comment ties
    // each declaration to its original entry.
    // menu_core.cpp
    std::uint32_t run_command(std::uint32_t a0); // 801c531c
    void field_menu_loop(); // 801c55a0
    void title_file_loop(); // 801c58ec
    void card_state_block(std::uint32_t a0); // 801c5b54
    void party_block(std::uint32_t a0); // 801c5bb8
    void screen_image_block(std::uint32_t a0); // 801c5c1c
    void block_354(std::uint32_t a0); // 801c5c80
    void table_directory_block(std::uint32_t a0); // 801c5ce4
    void block_340(std::uint32_t a0); // 801c5d48
    void block_344(std::uint32_t a0); // 801c5dac
    void primitive_block(std::uint32_t a0); // 801c5e10
    void field_blocks(std::uint32_t a0); // 801c5e74
    void allocate_blocks(); // 801c5f10
    void leave_menu(); // 801c5fe4
    void run_menu_mode(); // 801c62a8
    void reset_card_state(); // 801c6400
    void load_resources(); // 801c65f4
    void set_up_party(); // 801c6aa0
    void reset_buffer_index(); // 801c6d4c
    void reset_load_state(); // 801c6d5c
    void load_white_clut(); // 801c6d90
    void set_up_labels(); // 801c6e0c
    void read_sheet_entries(); // 801c6e68
    void set_up_frame_primitives(); // 801c6f70
    void set_up_screen(); // 801c7b0c
    void menu_frame(); // 801c7bf4
    void decode_input(); // 801c7d78
    void split_play_time(std::uint32_t a0); // 801c7f34
    void set_gradient_quad(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801c8164
    void check_loaded_disc(std::uint32_t a0); // 801c8694
    void link_fade(); // 801d1258
    void draw_screen(); // 801d1ca0
    void update_view(); // 801d1d40
    void zoom_in(); // 801d1e80
    void zoom_out(); // 801d1eb0
    void hide_cursors(); // 801d22c4
    void set_markers(std::uint32_t a0); // 801d22f4
    void clear_markers(); // 801d2484
    void draw_status_panel(); // 801d2968
    void open_field_menu(); // 801d2d38
    std::uint32_t sound_mode_screen(); // 801d9808
    void draw_file_labels(); // 801d9f34
    std::uint32_t file_screen(std::uint32_t a0, std::uint32_t a1); // 801d9f98

    // menu_text.cpp
    void split_decimal_digits(std::uint32_t a0); // 801c80b8
    void set_screen_quad_vectors(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4); // 801c851c
    void draw_highlight(std::uint32_t a0, std::uint32_t a1); // 801d1ee0
    void prepare_window_packets(std::uint32_t a0); // 801e53cc
    void set_label_packets(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801e7c50
    void layout_label_pairs(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801e7e68
    void layout_labels_row4(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2); // 801e8018
    void clear_bytes(std::uint32_t a0, std::uint32_t a1); // 801e8044
    void place_label(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4, std::uint32_t a5, std::uint32_t a6, std::uint32_t a7); // 801e8070
    void reveal_sprite_columns(std::uint32_t a0, std::uint32_t a1); // 801e8474
    void reveal_row_list(std::uint32_t a0); // 801e86c8
    void build_sprite_columns(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2); // 801e8978
    void build_row_list(std::uint32_t a0); // 801e8b4c
    void load_character_name(std::uint32_t a0, std::uint32_t a1); // 801e8da8
    void shade_quad(std::uint32_t a0, std::uint32_t a1); // 801e8eac
    void shade_window_quads(std::uint32_t a0, std::uint32_t a1); // 801e8f60
    void set_quad_translucent(std::uint32_t a0); // 801e91c4
    void set_quad_rect(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4, std::uint32_t a5, std::uint32_t a6); // 801e920c
    void init_text_quad(std::uint32_t a0); // 801e927c

    // menu_windows.cpp
    void project_quads(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801ce198
    void draw_packets(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2); // 801ce2b4
    void draw_header(); // 801ce338
    void draw_cursors(); // 801ce3c8
    void draw_lists_340_344(); // 801ce464
    void draw_party_windows(); // 801ce540
    void draw_status_quads(); // 801ce660
    void draw_rows_35c(); // 801ce860
    void draw_row_block(); // 801ceb5c
    void draw_windows_360(); // 801cebb4
    void draw_screen_image(); // 801cec40
    void draw_lists_354(); // 801cf308
    void draw_slot_cursors(); // 801cf37c
    void draw_slot_icons(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2); // 801cf5e4
    void draw_card_headers(); // 801cf8d8
    void draw_slot_lines(); // 801cfb48
    void draw_scroll_strip(); // 801cff64
    void draw_file_screen(); // 801d01d0
    void draw_slot_list(); // 801d02d8
    void project_panel_piece(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801d0954
    void draw_panel(std::uint32_t a0, std::uint32_t a1); // 801d09f0
    void draw_panels(); // 801d0c78
    void draw_state_windows_4e0(); // 801d0d90
    std::uint32_t delay_seven(); // 801d0e20
    void draw_state_windows_ae0(); // 801d0e38
    std::uint32_t delay_five(); // 801d0ebc
    void draw_state_windows_10e0(); // 801d0ed4
    void draw_state_windows_14e0(); // 801d0f54
    void draw_state_window_17e0(); // 801d0fd4
    void draw_windows_1de0(); // 801d1030
    void draw_state_windows_18e0(); // 801d10dc
    void draw_state_windows_1be0(); // 801d1160
    void draw_state_windows(); // 801d11f0
    void draw_list_window(std::uint32_t a0, std::uint32_t a1); // 801d12d4
    void draw_list_windows(); // 801d13f8
    void draw_window_43c(); // 801d1464
    void draw_quads_440(); // 801d14b0
    void draw_rows_42c(); // 801d14fc
    void draw_rows_430(); // 801d1640
    void draw_rows_434(); // 801d17c4
    void draw_rows_438(); // 801d1914
    void draw_windows_444(); // 801d1aac
    void draw_field_menu_screen(); // 801d1b20
    void draw_title_file_screen(); // 801d1be8
    void grow_opening_panels(); // 801d3b00
    void build_panel_title(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4); // 801d3c4c
    void build_panel_corners(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4); // 801d3db0
    void build_panel_top_edge(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801d3ff8
    void build_panel_bottom_edge(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4); // 801d433c
    void build_panel_left_edge(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801d4688
    void build_panel_right_edge(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4); // 801d49d0
    void build_panel(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4, std::uint32_t a5, std::uint32_t a6, std::uint32_t a7); // 801d4d1c

    // menu_fieldmenu.cpp
    void start_slide(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4, std::uint32_t a5); // 801c81e0
    void step_slide(std::uint32_t a0); // 801c8324
    void play_menu_sound(std::uint32_t a0); // 801c8574
    void open_amount_window(); // 801d28a8
    void open_play_time_window(); // 801d28fc
    void slide_party_panels(std::uint32_t a0, std::uint32_t a1); // 801d29a8
    void open_window(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4, std::uint32_t a5, std::uint32_t a6, std::uint32_t a7, std::uint32_t a8); // 801d397c
    void draw_panel_portrait(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801d4f2c
    void draw_panel_labels(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2); // 801d50ec
    void draw_panel_numbers_4c_4e(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801d51ec
    void draw_panel_numbers_50_52(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801d53d0
    void draw_panel_numbers_44_48(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801d55b4
    void draw_panel_numbers_62_63(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801d5794
    void draw_party_panel(std::uint32_t a0, std::uint32_t a1); // 801d5a50
    void draw_amount(std::uint32_t a0, std::uint32_t a1); // 801d5ba4
    void draw_play_time(std::uint32_t a0, std::uint32_t a1); // 801d5cf8
    void draw_detail_portrait(std::uint32_t a0, std::uint32_t a1); // 801d5ed4
    void draw_detail_numbers(std::uint32_t a0, std::uint32_t a1); // 801d680c
    void build_file_slot_marker(std::uint32_t a0); // 801e56e8
    void build_file_slot_outline(std::uint32_t a0); // 801e5924
    void build_file_slots(); // 801e5acc
    void build_file_glyphs(); // 801e5b88
    void build_file_banner(); // 801e5e4c
    void build_file_select_panels(); // 801e6450

    // menu_screens.cpp
    void status_panel_layout_sprites(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4, std::uint32_t a5); // 801cd81c
    void status_panel_byte62_digits(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4); // 801cdb1c
    void status_panel_values(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4, std::uint32_t a5); // 801cdc6c
    void status_panel_values_tail(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801ce024
    void build_status_panel(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4, std::uint32_t a5); // 801ce0cc
    void release_help_block(); // 801d3674
    void open_item_screen(); // 801da4a8
    void build_item_list(std::uint32_t a0); // 801da5bc
    void show_item_description(std::uint32_t a0, std::uint32_t a1); // 801da9a8
    void show_item_screen_texts(std::uint32_t a0); // 801db39c
    void build_target_panels(std::uint32_t a0); // 801db5e4
    std::uint32_t use_item_on_targets(std::uint32_t a0, std::uint32_t a1); // 801db920
    void measure_item_list(std::uint32_t a0, std::uint32_t a1); // 801dbdb4
    std::uint32_t item_screen(); // 801dbe54
    std::uint32_t apply_consumable(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2); // 801e31c0

    // menu_equip.cpp
    void load_menu_data_set(std::uint32_t a0); // 801c72bc
    std::uint32_t party_slot_bit(std::uint32_t a0, std::uint32_t a1); // 801c865c
    void draw_screen_title(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2); // 801d3344
    void release_screen_title(); // 801d3444
    void draw_party_window_sprites(std::uint32_t a0, std::uint32_t a1); // 801d3488
    void draw_portrait_panel(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801d36e0
    void release_text_blocks(std::uint32_t a0); // 801d4ea0
    void draw_stat_names(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2); // 801d7f50
    void make_stat_bar(std::uint32_t a0, std::uint32_t a1); // 801d827c
    void tint_sprite_parts(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801d83ac
    void measure_stat_bar(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2); // 801d84b4
    std::uint32_t largest_stat(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2); // 801d85dc
    void draw_stat_bars(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4); // 801d8644
    void draw_stat_panel(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801d8de4
    void draw_part_panel(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801d8ea4
    void close_equipment_shared(); // 801da518
    void open_cursor_sprite(std::uint32_t a0); // 801db02c
    void draw_cursor_sprite(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801db0a8
    void release_cursor_sprite(std::uint32_t a0); // 801db340
    void open_equipment_list(std::uint32_t a0); // 801de2c8
    void draw_equipment_frames(std::uint32_t a0, std::uint32_t a1); // 801de474
    std::uint32_t build_candidate_list(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4); // 801de5cc
    std::uint32_t commit_equipment(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801df0d4
    void keep_equipped_parts(std::uint32_t a0, std::uint32_t a1); // 801df5d0
    void preview_candidate(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4, std::uint32_t a5); // 801dfb68
    void draw_part_description(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4, std::uint32_t a5, std::uint32_t a6); // 801dff5c
    void equipment_screen_loop(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2); // 801e05d0
    std::uint32_t run_equipment_screen(std::uint32_t a0, std::uint32_t a1); // 801e0f78
    void rebuild_equipment_bonuses(std::uint32_t a0, std::uint32_t a1); // 801e36d4
    void update_equipment_stats(std::uint32_t a0, std::uint32_t a1); // 801e3a80

    // menu_card.cpp
    void undeliver_card_events(); // 801c87c4
    std::uint32_t wait_card_event(); // 801c881c
    std::uint32_t card_status(std::uint32_t a0); // 801c891c
    void close_card_events(); // 801c8960
    std::uint32_t poll_card_port(std::uint32_t a0); // 801c8a10
    void poll_cards(); // 801c8bec
    std::uint32_t list_card_directory(std::uint32_t a0); // 801c8d78
    void list_unscanned_ports(); // 801c8ee8
    std::uint32_t read_file_head(std::uint32_t a0, std::uint32_t a1); // 801c9038
    void load_file_head(std::uint32_t a0, std::uint32_t a1); // 801c90b0
    void mark_game_files(std::uint32_t a0); // 801c9270
    std::uint32_t refresh_cards(); // 801c93a8
    std::uint32_t cursor_slot_suits(std::uint32_t a0); // 801c9bcc
    std::uint32_t find_suitable_slot(std::uint32_t a0); // 801c9d34
    void card_cursor_down(std::uint32_t a0, std::uint32_t a1); // 801c9ef4
    std::uint32_t card_cursor_input(std::uint32_t a0); // 801ca750
    void build_save_title(std::uint32_t a0); // 801ca8c0
    std::uint32_t confirm_choice(std::uint32_t a0); // 801caa38
    std::uint32_t ask_confirmation(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2); // 801cacf8
    void reset_card_cursor(); // 801cadb0
    void card_access_indicator(std::uint32_t a0); // 801cae08
    void close_access_indicator(std::uint32_t a0); // 801cb0a8
    void decode_game_names(); // 801cb184
    void apply_loaded_payload(std::uint32_t a0); // 801cb28c
    std::uint32_t load_game(); // 801cb304
    std::uint32_t ask_format_card(std::uint32_t a0); // 801cb8ac
    std::uint32_t choose_save_digit(std::uint32_t a0, std::uint32_t a1); // 801cb9e8
    void build_save_payload(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2); // 801cba4c
    std::uint32_t save_game(std::uint32_t a0); // 801cbd90
    std::uint32_t save_game_body(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3); // 801cbdbc
    std::uint32_t run_file_command(std::uint32_t a0); // 801cd710
    void show_message(std::uint32_t a0); // 801d2f4c
    void close_message(); // 801d32b4
    void restart_card_access(); // 801d9b08
    std::uint32_t enter_card_mode(); // 801d9c84
    void leave_card_mode(); // 801d9e3c
    void leave_menu_screen(std::uint32_t a0); // 801e3088
    void recompute_gear_engine(std::uint32_t a0, std::uint32_t a1); // 801e41c0
    void recompute_gear_model(std::uint32_t a0, std::uint32_t a1); // 801e4258
    void recompute_gear_frame(std::uint32_t a0, std::uint32_t a1); // 801e42ac
    void recompute_gear_parts(std::uint32_t a0, std::uint32_t a1); // 801e433c
    std::uint32_t gear_part_level(std::uint32_t a0); // 801e4928
    void store_payload_game_data(std::uint32_t a0); // 801e4a28
    void restore_payload_game_data(std::uint32_t a0, std::uint32_t a1); // 801e4d10
    void release_file_blocks(); // 801e5b3c
    void draw_details_labels(); // 801e61b0
    void release_details_block(); // 801e649c
    void clear_file_title(); // 801e64e0
    void narrow_glyph_rows(std::uint32_t a0); // 801e6544
    std::uint32_t kanji_glyph_address(std::uint32_t a0); // 801e65e4
    void draw_file_title(std::uint32_t a0); // 801e6668
    void draw_details_time(std::uint32_t a0); // 801e68ac
    void draw_details_portrait(std::uint32_t a0, std::uint32_t a1); // 801e6ae8
    void draw_details_level(std::uint32_t a0, std::uint32_t a1); // 801e6b70
    void draw_details_hp(std::uint32_t a0, std::uint32_t a1); // 801e6cfc
    void draw_details_values(std::uint32_t a0, std::uint32_t a1); // 801e6f5c
    void draw_details_name(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2); // 801e71b4
    void build_title_strip(); // 801e733c
    void draw_file_details(std::uint32_t a0); // 801e76ec
    void show_file_details(std::uint32_t a0, std::uint32_t a1); // 801e781c
    void load_file_icon(std::uint32_t a0); // 801e78c8

  private:
    std::size_t services_at_start_{};
    std::uint32_t positions_{};
    [[nodiscard]] std::size_t services_left() const;
    std::uint32_t sp_;
    std::uint32_t frame_size_{};
    std::uint32_t entry_sp_;
    std::uint32_t game_start_{};
    [[nodiscard]] std::uint8_t *stack_byte(std::uint32_t address) const;
    [[nodiscard]] std::uint8_t *menu_byte(std::uint32_t address, std::uint32_t width) const;
};

} // namespace xem::reconstruction::menu
