// Menu overlay 82f84a24... (801c5000): the menu mode's entry and teardown,
// its heap blocks and resources, the frame and input decode, the top-level
// command loops (field menu, title file screen) and the sound-mode and file
// screens.
#include "xem/reconstruction/menu_overlay.hpp"

#include "xem/reconstruction/field_gte.hpp"
#include "xem/reconstruction/resident_text.hpp"

namespace xem::reconstruction::menu {
namespace {
// Menu state fields (offsets from *800625a0).
constexpr std::uint32_t draw_environment = 0x1d4; // current buffer's environment block
constexpr std::uint32_t buffer_blocks = 0x6c;     // first of the two environment blocks
constexpr std::uint32_t second_block = 0x120;     // second environment block
constexpr std::uint32_t view_angles = 0x1d8;      // SVECTOR: 3D view rotation
constexpr std::uint32_t view_offset = 0x1e0;      // VECTOR: translation (z = distance +1e8)
constexpr std::uint32_t view_distance = 0x1e8;
constexpr std::uint32_t view_matrix = 0x1f0;   // MATRIX built from the angles and offset
constexpr std::uint32_t frame_counter = 0x2d8; // frames since last cleared
constexpr std::uint32_t sheet = 0x2dc;         // sprite sheet (unpacked resource 3)
constexpr std::uint32_t labels = 0x2e0;        // label text (unpacked resource 4)
constexpr std::uint32_t effect_bank = 0x2e4;   // the menu's effect bank (8006259c)
constexpr std::uint32_t buffer_index = 0x308;  // 0/1: the buffer being built
constexpr std::uint32_t members = 0x30c;       // 16 bytes: character present in the party
constexpr std::uint32_t input_code = 0x325;    // decoded input of this frame
constexpr std::uint32_t card_poll_timer = 0x326;
constexpr std::uint32_t drawing = 0x327;     // nonzero draws the screen each frame
constexpr std::uint32_t view_motion = 0x329; // 0 still, 1/2 moving, 3/4 start a move
constexpr std::uint32_t sounds = 0x32a;      // nonzero plays menu effects
constexpr std::uint32_t party_count = 0x32b;
constexpr std::uint32_t card_state = 0x32c; // card state block (5034 bytes)
constexpr std::uint32_t tables = 0x330;     // data table directory (cc bytes)
constexpr std::uint32_t cards_present = 0x334;
constexpr std::uint32_t b_335 = 0x335;
constexpr std::uint32_t cursor = 0x336;       // top command cursor
constexpr std::uint32_t cursor_shown = 0x337; // cursor the labels were last drawn for
constexpr std::uint32_t choice = 0x338;       // screen choice
constexpr std::uint32_t choice_shown = 0x339; // choice the labels were last drawn for
constexpr std::uint32_t choice_count = 0x33a;
constexpr std::uint32_t fighters = 0x33b;          // party members with a gear (+a0 not ff)
constexpr std::uint32_t party = 0x33c;             // party block (6c bytes)
constexpr std::uint32_t block_340 = 0x340;         // field-menu block (328 bytes)
constexpr std::uint32_t block_344 = 0x344;         // field-menu block (374 bytes)
constexpr std::uint32_t primitives = 0x348;        // shared primitive block (15c bytes)
constexpr std::uint32_t screen_images = 0x350;     // block with the screen copy rectangle (+1180)
constexpr std::uint32_t block_354 = 0x354;         // 140c bytes
constexpr std::uint32_t portraits = 0x364;         // two 720-byte blocks
constexpr std::uint32_t portrait_marks = 0x380;    // two 18-byte blocks
constexpr std::uint32_t field_block_slots = 0x39c; // three 127c-byte blocks
constexpr std::uint32_t markers = 0x428;           // 14c bytes
constexpr std::uint32_t sheet_entries = 0x46c;     // four records of six words (80026338)
constexpr std::uint32_t load_state = 0x4d8;
constexpr std::uint32_t first_member = 0x4dc; // first occupied party slot
constexpr std::uint32_t label_images = 0x4e0; // label image records (801e7e68)
constexpr std::uint32_t label_pixels = 0x558; // 38e-byte label pixel block
constexpr std::uint32_t command_labels = 0x6e0;
constexpr std::uint32_t file_labels = 0xde0;
constexpr std::uint32_t sound_labels = 0x1be0;
// Environment block: draw environment +0, display environment +5c, ordering
// tables at +70 (16 entries; the frame's primitives), +90 and +ac (the list sent).
constexpr std::uint32_t display_environment = 0x5c;
constexpr std::uint32_t ordering_table = 0x70;
constexpr std::uint32_t top_table = 0x90;
constexpr std::uint32_t sent_table = 0xac;
// Party block (*(state + 33c)).
constexpr std::uint32_t party_ids = 0x30; // 3 character ids, ff empty
constexpr std::uint32_t party_ready = 0x60;
// Card state block (*(state + 32c)).
constexpr std::uint32_t card_scan_done = 0x4f88; // per port
constexpr std::uint32_t card_ports = 0x4fe4;     // per port: card present
constexpr std::uint32_t card_mode = 0x4fe6;
// Resident words.
constexpr std::uint32_t pressed_buttons = 0x800594a4; // last dequeued entry (80035cdc)
constexpr std::uint32_t pressed_buttons_2 = 0x8005948c;
constexpr std::uint32_t play_frames = 0x80059488;
constexpr std::uint32_t menu_mode = 0x80059460;   // 0 field menu, 2 title file screen, 6 other
constexpr std::uint32_t menu_cursor = 0x800594cc; // field menu cursor kept between openings
constexpr std::uint32_t triangle_menu = 0x80059171;
constexpr std::uint32_t menu_effects = 0x80059178;
constexpr std::uint32_t menu_resources = 0x8005945c;
constexpr std::uint32_t menu_effect_bank = 0x8006259c;
constexpr std::uint32_t load_resume = 0x800594d0; // 0, 1 (title timeout) or 2 (loaded)
constexpr std::uint32_t character_gear = 0xa0;    // character record + a0: gear, ff none
constexpr std::uint32_t party_order = 0x8006f368; // 3 character ids of the game data
constexpr std::uint32_t members_joined = 0x8006f364;
constexpr std::uint32_t members_active = 0x8006f366;
constexpr std::uint32_t save_count = 0x8006ef64;  // game data halfword: line of the label text
constexpr std::uint32_t loaded_disc = 0x8006f008; // game data: disc of the loaded file
// Overlay statics.
constexpr std::uint32_t reset_enabled = 0x801e9784; // 1 checks the reset combination
constexpr std::uint32_t in_command = 0x801e977a;
constexpr std::uint32_t card_message = 0x801e9778; // nonzero shows the card notice
constexpr std::uint32_t save_file_screen = 0x801e96a4;

std::uint32_t low8(std::uint32_t value) { return value & 0xffU; }
} // namespace

// ---- Mode entry and teardown -------------------------------------------------

// 801c62a8: the menu mode. Allocate the blocks (801c5f10), set up the
// screen (801c7b0c) and enable drawing and sounds, then run the menu of
// 80059460: 0 the field menu (801d2d38, 801c55a0), 2 the title file screen
// (801c58ec) followed by the disc check for the loaded file (801c8694), 6
// 801c57a4; then tear down (801c5fe4).
void Overlay::run_menu_mode() {
    const auto stack_frame = enter(0x18);
    allocate_blocks();
    set_up_screen();
    put8(at(drawing), 1);
    put8(at(sounds), 1);
    const auto mode = u8(menu_mode);
    if (mode == 2) {
        put8(load_resume, 0);
        title_file_loop();
        const auto party_block = u32(at(party));
        put8(party_block + 9, 0);
        put8(party_block + 4, 0);
        put8(party_block + 3, 0);
        const auto resume = u8(load_resume);
        if (resume == 0)
            check_loaded_disc(0);
        else if (resume == mode)
            check_loaded_disc(u8(loaded_disc));
    } else if (mode == 0) {
        open_field_menu();
        field_menu_loop();
    } else if (mode == 6) {
        missing("menu_mode", 0x801c63dc, "symbol:menu-801c57a4",
                "Menu mode 6 (801c57a4) is not recovered");
    }
    leave_menu();
}

// 801c5f10: allocate the blocks every menu uses (party 801c5bb8, screen
// images 801c5c1c, 801c5c80, the data table directory 801c5ce4, primitives
// 801c5e10) and the marker block (state + 428, 14c bytes), then the card
// state (801c5b54) for the title file screen and mode 6, and for the field
// menu also the field blocks (801c5d48, 801c5dac, 801c5e74).
void Overlay::allocate_blocks() {
    const auto stack_frame = enter(0x18);
    party_block(1);
    screen_image_block(1);
    block_354(1);
    table_directory_block(1);
    primitive_block(1);
    const auto block = allocate(0x14c, 0, 0x801c5f44);
    put32(at(markers), block);
    static_cast<void>(bzero(block, 0x14c));
    const auto mode = u8(menu_mode);
    if (mode == 0) {
        card_state_block(1);
        block_340(1);
        block_344(1);
        field_blocks(1);
    } else if (mode == 2 || mode == 6) {
        card_state_block(1);
    }
}

namespace {
// One menu block: with `create` (a byte) nonzero allocate `size` bytes (JAL
// at `allocate_site`), zero them and keep the address at menu state +
// `slot`; with zero release it (JAL at `release_site`).
struct BlockSpec {
    std::uint32_t slot, size, allocate_site, release_site;
};
constexpr BlockSpec card_spec{card_state, 0x5034, 0x801c5b68, 0x801c5ba0};
constexpr BlockSpec party_spec{party, 0x6c, 0x801c5bcc, 0x801c5c04};
constexpr BlockSpec screen_spec{screen_images, 0x1194, 0x801c5c30, 0x801c5c68};
constexpr BlockSpec block_354_spec{block_354, 0x140c, 0x801c5c94, 0x801c5ccc};
constexpr BlockSpec tables_spec{tables, 0xcc, 0x801c5cf8, 0x801c5d30};
constexpr BlockSpec block_340_spec{block_340, 0x328, 0x801c5d5c, 0x801c5d94};
constexpr BlockSpec block_344_spec{block_344, 0x374, 0x801c5dc0, 0x801c5df8};
constexpr BlockSpec primitive_spec{primitives, 0x15c, 0x801c5e24, 0x801c5e5c};
} // namespace

namespace {
void menu_block(Overlay &overlay, std::uint32_t create, const BlockSpec &spec) {
    if (low8(create) == 0) {
        overlay.release(overlay.u32(overlay.at(spec.slot)), spec.release_site);
        return;
    }
    const auto block = overlay.allocate(spec.size, 0, spec.allocate_site);
    overlay.put32(overlay.at(spec.slot), block);
    static_cast<void>(overlay.bzero(block, static_cast<std::int32_t>(spec.size)));
}
} // namespace

// 801c5b54: the card state block (state + 32c, 5034 bytes).
void Overlay::card_state_block(std::uint32_t a0) { menu_block(*this, a0, card_spec); }
// 801c5bb8: the party block (state + 33c, 6c bytes).
void Overlay::party_block(std::uint32_t a0) { menu_block(*this, a0, party_spec); }
// 801c5c1c: the screen image block (state + 350, 1194 bytes).
void Overlay::screen_image_block(std::uint32_t a0) { menu_block(*this, a0, screen_spec); }
// 801c5c80: the block at state + 354 (140c bytes).
void Overlay::block_354(std::uint32_t a0) { menu_block(*this, a0, block_354_spec); }
// 801c5ce4: the data table directory (state + 330, cc bytes).
void Overlay::table_directory_block(std::uint32_t a0) { menu_block(*this, a0, tables_spec); }
// 801c5d48: the field-menu block at state + 340 (328 bytes).
void Overlay::block_340(std::uint32_t a0) { menu_block(*this, a0, block_340_spec); }
// 801c5dac: the field-menu block at state + 344 (374 bytes).
void Overlay::block_344(std::uint32_t a0) { menu_block(*this, a0, block_344_spec); }
// 801c5e10: the primitive block (state + 348, 15c bytes).
void Overlay::primitive_block(std::uint32_t a0) { menu_block(*this, a0, primitive_spec); }

// 801c5e74: the three field blocks (state + 39c.., 127c bytes each).
void Overlay::field_blocks(std::uint32_t a0) {
    const auto stack_frame = enter(0x18);
    for (std::uint32_t i = 0; i < 3; ++i) {
        if (low8(a0) == 0) {
            release(u32(at(field_block_slots) + 4 * i), 0x801c5ee8);
            continue;
        }
        const auto block = allocate(0x127c, 0, 0x801c5e90);
        put32(at(field_block_slots) + 4 * i, block);
        static_cast<void>(bzero(block, 0x127c));
    }
}

// 801c5fe4: leave the menu. The field menu keeps its cursor for the next
// opening (800594cc) unless it was opened by triangle (80059171). Four more
// frames with drawing stopped from the third, until the buffer index is 0;
// then release every block, the sheet, labels and label pixels; with the
// menu effect bank loaded (80059178) stop its effects (8003a094) and unlink
// it (8003852c) a frame apart and release it; finally the mode's blocks and
// the menu state itself.
void Overlay::leave_menu() {
    const auto stack_frame = enter(0x18);
    if (u8(menu_mode) == 0) {
        hide_cursors();
        slide_party_panels(0, 0);
        put8(u32(at(party)) + 6, 0);
        put8(u32(at(party)) + 5, 0);
        if (u8(triangle_menu) == 0)
            put8(menu_cursor, u8(at(cursor)));
    }
    menu_frame();
    menu_frame();
    put8(at(drawing), 0);
    menu_frame();
    do
        menu_frame();
    while (u32(at(buffer_index)) != 0);
    party_block(0);
    screen_image_block(0);
    block_354(0);
    table_directory_block(0);
    primitive_block(0);
    release(u32(at(markers)), 0x801c60fc);
    release(u32(at(sheet)), 0x801c6114);
    release(u32(at(labels)), 0x801c612c);
    release(u32(at(label_pixels)), 0x801c6144);
    if (u8(menu_effects) != 0)
        missing("menu_teardown", 0x801c6170, "symbol:sound-effect-bank-unlink-8003852c",
                "Stopping and unlinking the menu effect bank (8003a094, 8003852c) is not "
                "recovered");
    const auto mode = u8(menu_mode);
    if (mode == 0) {
        card_state_block(0);
        block_340(0);
        block_344(0);
        field_blocks(0);
        release(u32(at(portraits)), 0x801c6228);
        release(u32(at(portrait_marks)), 0x801c6240);
        release(u32(at(portraits) + 4), 0x801c6258);
        release(u32(at(portrait_marks) + 4), 0x801c6270);
    } else if (mode == 2 || mode == 6) {
        card_state_block(0);
    }
    release(state(), 0x801c6290);
}

// ---- Screen setup -------------------------------------------------------------

// 801c7b0c: the screen copy rectangle (2c0, 100, 140 x e0 at screen block +
// 1180), the primitive block's line color byte (+15b: 40, 4c on the title
// file screen), then the party and resources (801c6aa0), buffer 0 (801c6d4c),
// the labels (801c6e0c), the frame primitives (801c6f70) and the sheet
// entries (801c6e68); the card state (801c6400) and load state (801c6d5c)
// for modes 0, 2 and 6.
void Overlay::set_up_screen() {
    const auto stack_frame = enter(0x18);
    const auto images = u32(at(screen_images));
    put16(images + 0x1180, 0x2c0);
    put16(images + 0x1182, 0x100);
    put16(images + 0x1184, 0x140);
    put16(images + 0x1186, 0xe0);
    put8(u32(at(primitives)) + 0x15b, 0x40);
    set_up_party();
    reset_buffer_index();
    set_up_labels();
    set_up_frame_primitives();
    read_sheet_entries();
    const auto mode = u8(menu_mode);
    if (mode == 2)
        put8(u32(at(primitives)) + 0x15b, 0x4c);
    if (mode == 0 || mode == 2 || mode == 6) {
        reset_card_state();
        reset_load_state();
    }
}

// 801c6aa0: the party. The top cursor starts at the kept field menu cursor
// (field menu not opened by triangle) or 1. Characters present are those
// whose party bit (801c865c over 8006f364 & 8006f366, 11 bits) is set; the
// three party slots take the game data's order (8006f368) where present,
// counting members (+32b) and gear pilots (+33b, party +60). +4dc is the
// first occupied slot. Then the resources (801c65f4).
void Overlay::set_up_party() {
    const auto stack_frame = enter(0x20);
    put8(at(cursor), u8(menu_mode) == 0 && u8(triangle_menu) == 0 ? u8(menu_cursor) : 1U);
    put8(at(cursor_shown), 0xff);
    put8(at(card_poll_timer), 0x3c);
    put8(at(cards_present), 0);
    put8(at(b_335), 0);
    put8(at(party_count), 0);
    const auto bits = (u16(members_joined) & u16(members_active)) & 0x7ffU;
    for (std::uint32_t character = 0; character < 16; ++character)
        put8(at(members) + character,
             (party_slot_bit(bits & 0xffffU, character) & 0xffffU) != 0 ? 1U : 0U);
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        const auto block = u32(at(party));
        put8(block + party_ready + slot, 0);
        const auto id = u8(party_order + slot);
        if (id == 0xff || u8(at(members) + id) == 0) {
            put8(block + party_ids + slot, 0xff);
            continue;
        }
        put8(block + party_ids + slot, id);
        put8(at(party_count), u8(at(party_count)) + 1);
        const auto member = u32(at(party)) + slot;
        if (u8(character_records + u8(member + party_ids) * 0xa4 + character_gear) != 0xff) {
            put8(member + party_ready, 1);
            put8(at(fighters), u8(at(fighters)) + 1);
        }
    }
    for (std::uint32_t slot = 0; slot < 3; ++slot)
        if (u8(u32(at(party)) + party_ids + slot) != 0xff) {
            put8(at(first_member), slot);
            break;
        }
    load_resources();
}

// 801c65f4: the menu resources in the block 8005945c names (a table of
// offsets that 8003342c relocates): resource 1 is the file icon TIM, whose
// CLUT and pixels become the card header template (card state + 4b94: "SC",
// 11, 1, then the title area, palette +60 and icon +80) along with two
// 13-byte records copied from 801c5028/801c5038; resource 2 goes to
// 8002dd20; resources 3 and 4 are the sprite sheet (+2dc) and the label text
// (+2e0); resource 5 holds a TIM per character, loaded for the three party
// members at the CLUT and image positions of sheet entry 14c's record. With
// the menu effects enabled (80059178) the effect bank is read from disc
// (directory 12, file 5) and linked (80038428). The resource block is
// released.
void Overlay::load_resources() {
    const auto stack_frame = enter(0xa8);
    const auto resources = u32(menu_resources);
    static_cast<void>(resident::relocate_offsets(*this, resources));    // 8003342c
    const auto unpack = [&](std::uint32_t source, std::uint32_t mode) { // 80032e88
        return resident::unpack_to_new_block(
            *this, source, mode, [&](std::uint32_t size, std::uint32_t kind) {
                return allocate(size, kind, resident::unpack_allocation_site);
            });
    };
    const auto icon = unpack(u32(resources + 4), 1);
    open_tim(icon);
    const auto card_block_address = u32(at(card_state));
    static_cast<void>(read_tim(card_block_address + 0xb80));
    // Two unaligned 13-byte records from the overlay statics.
    for (std::uint32_t i = 0; i < 13; ++i) {
        put8(card_block_address + 0x4fce + i, u8(0x801c5028 + i));
        put8(card_block_address + 0x501c + i, u8(0x801c5038 + i));
    }
    const auto header = u32(at(card_state)) + 0x4b94;
    put8(header, 0x53);
    put8(header + 1, 0x43);
    put8(header + 2, 0x11);
    put8(header + 3, 1);
    static_cast<void>(bzero(header + 4, 0x5c));
    static_cast<void>(memmove(header + 0x60, u32(u32(at(card_state)) + 0xb88), 0x20));
    static_cast<void>(memmove(header + 0x80, u32(u32(at(card_state)) + 0xb90), 0x80));
    release(icon, 0x801c67a8);
    const auto second = unpack(u32(resources + 8), 1);
    resident::load_tim_list(
        *this, second, [&] { draw_sync(); },
        [&](std::uint32_t rect, std::uint32_t data) { load_image(rect, data); }); // 8002dd20
    release(second, 0x801c67c8);
    put32(at(sheet), unpack(u32(resources + 12), 0));
    put32(at(labels), unpack(u32(resources + 16), 0));
    auto locals = frame(0xa8);
    // Sheet entry records (80026338's six outputs) on this frame: e0 and
    // then 14b at +38, 14c at +50, 14d at +68. The portrait positions of
    // party slot k are the CLUT x/y and image x/y words at +40 + 18 * k.
    const auto entry = [&](std::uint32_t index, std::uint32_t record) {
        resident::sheet_part_texture(*this, u32(at(sheet)), index, locals[record],
                                     locals[record + 4], locals[record + 8], locals[record + 0xc],
                                     locals[record + 0x10],
                                     locals[record + 0x14]); // 80026338
    };
    entry(0xe0, 0x38);
    entry(0x14b, 0x38);
    entry(0x14c, 0x50);
    entry(0x14d, 0x68);
    put32(locals[0x60], u32(locals[0x60]) + 0xc);
    const auto faces = unpack(u32(resources + 0x14), 1);
    const auto image = locals[0x20];
    for (std::uint32_t slot = 0; slot < 3; ++slot) {
        const auto id = u8(u32(at(party)) + party_ids + slot);
        if (id == 0xff)
            continue;
        open_tim(faces + id * 0xb20);
        static_cast<void>(read_tim(image));
        const auto row = 0x18 * slot;
        put16(u32(image + 4), u16(locals[0x40] + row));
        put16(u32(image + 4) + 2, u16(locals[0x44] + row));
        put16(u32(image + 12), u16(locals[0x48] + row));
        put16(u32(image + 12) + 2, u16(locals[0x4c] + row));
        load_image(u32(image + 4), u32(image + 8));
        load_image(u32(image + 12), u32(image + 16));
    }
    draw_sync();
    release(faces, 0x801c69dc);
    if (u8(menu_effects) != 0) {
        static_cast<void>(select_directory(0x10, 2));
        const auto bank = allocate(file_words(5), 0, 0x801c6a0c);
        put32(menu_effect_bank, bank);
        static_cast<void>(read_file(5, bank, 0, 0x80));
        disc_wait(0);
        static_cast<void>(select_directory(0x10, 0));
        missing("menu_resources", 0x801c6a4c, "symbol:sound-effect-bank-link-80038428",
                "Linking the menu effect bank (80038428) is not recovered");
    }
    put32(at(effect_bank), u32(menu_effect_bank));
    release(resources, 0x801c6a68);
}

// 801c6d4c: start building buffer 0.
void Overlay::reset_buffer_index() { put32(at(buffer_index), 0); }

// 801c6e0c: the text palette at (0, 1d1) (80033698), the label pixel block
// (+558, 38e bytes), the four title command labels from 801ea524 (801e7e68)
// and the white CLUT entry (801c6d90).
void Overlay::set_up_labels() {
    const auto stack_frame = enter(0x18);
    load_text_palette(0, 0x1d1);
    const auto pixels = allocate(0x38e, 0, 0x801c6e24);
    put32(at(label_pixels), pixels);
    layout_label_pairs(at(label_images), 0x801ea524, 0, 4);
    load_white_clut();
}

// 801c6d90: a 16-entry CLUT row at (0, 1c0) whose second entry is white
// (7fff), loaded from a temporary block.
void Overlay::load_white_clut() {
    const auto stack_frame = enter(0x28);
    const auto block = allocate(0x20, 0, 0x801c6da0);
    static_cast<void>(bzero(block, 0x20));
    put16(block + 2, 0x7fff);
    auto locals = frame(0x28);
    const auto rect = locals[0x10];
    put16(rect, 0);
    put16(rect + 2, 0x1c0);
    put16(rect + 4, 0x10);
    put16(rect + 6, 1);
    load_image(rect, block);
    draw_sync();
    release(block, 0x801c6df0);
}

// 801c6e68: the sheet entries fe, 103, 100 and 101 (80026338) into four
// six-word records at state + 46c.
void Overlay::read_sheet_entries() {
    const auto stack_frame = enter(0x50);
    const std::array<std::uint32_t, 4> indices{0xfe, 0x103, 0x100, 0x101};
    for (std::uint32_t i = 0; i < 4; ++i) {
        const auto record = at(sheet_entries) + 0x18 * i;
        resident::sheet_part_texture(*this, u32(at(sheet)), indices[i], record, record + 4,
                                     record + 8, record + 0xc, record + 0x10, record + 0x14);
    }
}

// 801c6f70: per buffer, the frame primitives of the primitive block: a
// shaded backdrop quad (801c8164, semi-transparent, +50/+74), two dark green
// separator lines (+c8/+e0, +f8/+110), a grey full-screen fade quad (+98/+b0,
// semi-transparent) and two draw modes (+128/+134 texture page (140, 80);
// +140/+14c page (180, 0) with blend 2), both with a 100 x 100 window.
void Overlay::set_up_frame_primitives() {
    const auto stack_frame = enter(0x48);
    auto locals = frame(0x48);
    const auto window = locals[0x18];
    put16(window, 0);
    put16(window + 2, 0);
    put16(window + 4, 0x100);
    put16(window + 6, 0x100);
    hide_cursors();
    for (std::uint32_t k = 0; k < 2; ++k) {
        const auto base = [&] { return u32(at(primitives)); };
        set_gradient_quad(base() + 0x50 + 0x24 * k, 0x80, 0x80, 0);
        set_semi_trans(base() + 0x50 + 0x24 * k, 1);
        set_line_f3(base() + 0xc8 + 0x18 * k);
        put8(base() + 0xcc + 0x18 * k, 0);
        put8(base() + 0xcd + 0x18 * k, 0x40);
        put8(base() + 0xce + 0x18 * k, 0);
        set_line_f3(base() + 0xf8 + 0x18 * k);
        put8(base() + 0xfc + 0x18 * k, 0);
        put8(base() + 0xfd + 0x18 * k, 0x40);
        put8(base() + 0xfe + 0x18 * k, 0);
        const auto quad = base() + 0x98 + 0x18 * k;
        set_poly_f4(quad);
        const std::array<std::uint32_t, 8> corners{0, 0, 0x140, 0, 0, 0xe0, 0x140, 0xe0};
        for (std::uint32_t i = 0; i < 8; ++i)
            put16(quad + 8 + 2 * i, corners[i]);
        put8(quad + 4, 0x80);
        put8(quad + 5, 0x80);
        put8(quad + 6, 0x80);
        set_semi_trans(quad, 1);
        set_draw_mode(base() + 0x128 + 0xc * k, 0, 0, get_tpage(0, 0, 0x140, 0x80), window);
        set_draw_mode(base() + 0x140 + 0xc * k, 0, 0, get_tpage(0, 2, 0x180, 0), window);
    }
}

// 801c8164: a POLY_G4 at `a0` whose top corners have color (a1, a2, a3) and
// whose bottom corners are black.
void Overlay::set_gradient_quad(std::uint32_t a0, std::uint32_t a1, std::uint32_t a2,
                                std::uint32_t a3) {
    const auto stack_frame = enter(0x28);
    set_poly_g4(a0);
    for (const auto offset : {4U, 0xcU}) {
        put8(a0 + offset, a1);
        put8(a0 + offset + 1, a2);
        put8(a0 + offset + 2, a3);
    }
    for (const auto offset : {0x14U, 0x1cU}) {
        put8(a0 + offset, 0);
        put8(a0 + offset + 1, 0);
        put8(a0 + offset + 2, 0);
    }
}

// 801c6400: the card state: per port the scan flags (+4f88, +4f8a) clear and
// no entry (+4f8c ff), the card mode (+4fe6) 0, the 32 directory entries
// (+58 + 5c * i clear, +4fae + i ff). The file title (card state + 4ffc, 30
// bytes) is line 8006ef64 of the text file 1 of directory 11 (read into a
// temporary block; two-byte characters have a first byte of 80 or more,
// lines end with 0a); +501a and +501b are cleared.
void Overlay::reset_card_state() {
    const auto stack_frame = enter(0x20);
    for (std::uint32_t port = 0; port < 2; ++port) {
        const auto block = u32(at(card_state));
        put8(block + card_scan_done + port, 0);
        put8(block + 0x4f8a + port, 0);
        put8(block + 0x4f8c + port, 0xff);
    }
    put8(u32(at(card_state)) + card_mode, 0);
    for (std::uint32_t i = 0; i < 0x20; ++i) {
        put8(u32(at(card_state)) + 0x58 + 0x5c * i, 0);
        put8(u32(at(card_state)) + 0x4fae + i, 0xff);
    }
    auto line = u16(save_count);
    static_cast<void>(select_directory(0x10, 1));
    const auto text = allocate(file_words(1), 1, 0x801c6508);
    static_cast<void>(read_file(1, text, 0, 0x80));
    disc_wait(0);
    std::uint32_t offset = 0;
    for (; (line & 0xffffU) != 0; line = (line - 1U) & 0xffffU) {
        for (;;) {
            while (u8(text + offset) > 0x7f)
                offset += 2;
            if (u8(text + offset) == 0x0a)
                break;
            ++offset;
        }
        ++offset;
    }
    for (std::uint32_t i = 0; i < 0x1e; ++i)
        put8(u32(at(card_state)) + 0x4ffc + i, u8(text + offset + i));
    put8(u32(at(card_state)) + 0x501b, 0);
    put8(u32(at(card_state)) + 0x501a, 0);
    static_cast<void>(select_directory(0x10, 0));
    release(text, 0x801c65d0);
}

// 801c6d5c: clear the load state (+4d8, +4d9, +4cc, +4d0, +4d4).
void Overlay::reset_load_state() {
    put8(at(load_state), 0);
    put32(at(0x4cc), 0);
    put32(at(0x4d0), 0);
    put8(at(0x4d9), 0);
    put32(at(0x4d4), 0);
}

// ---- The frame -----------------------------------------------------------------

// 801c7bf4: one menu frame. Decode the input (801c7d78), check the reset
// combination when enabled (80019ca0), swap the draw buffers and clear this
// buffer's ordering table, advance rand (8001bd40), update the 3D view
// (801d1d40), count the frame and split the play time (801c7f34), draw the
// screen (801d2968, 801d1ca0), then wait for the GPU and the vertical blank,
// present the other buffer, copy the screen image, send the ordering table
// and poll the memory cards (801c8bec, 801c8ee8).
void Overlay::menu_frame() {
    const auto stack_frame = enter(0x18);
    // Arrivals before the frame entry, the boundary, then the entry itself.
    catch_up();
    if (boundary)
        boundary("frame_entry");
    pass_position(false);
    if (u32(u32(0x8005917c)) != 0xffffffffU)
        missing("menu_frame", 0x801c7c18, "symbol:menu-debug-break",
                "The diagnostic build's break is not a recovered result");
    decode_input();
    // 80019ca0: the reset combination (last dequeued buttons 90c) resets
    // through 80019cd0.
    if (u8(reset_enabled) != 0 && u16(0x80059570) == 0x90c)
        missing("menu_frame", 0x80019cb8, "symbol:soft-reset-80019cd0",
                "The soft reset (80019cd0) is not recovered");
    const auto state_block = state();
    const auto first = state_block + buffer_blocks;
    put32(state_block + draw_environment,
          u32(state_block + draw_environment) == first ? state_block + second_block : first);
    put32(state_block + buffer_index, u32(state_block + buffer_index) == 0 ? 1U : 0U);
    clear_otag_r(u32(state_block + draw_environment) + ordering_table, 0x10);
    static_cast<void>(random_range(0, 0xff));
    update_view();
    put32(at(frame_counter), u32(at(frame_counter)) + 1);
    split_play_time(u32(play_frames));
    draw_status_panel();
    draw_screen();
    const auto shown = u32(at(buffer_index)) == 0 ? 1U : 0U;
    draw_sync();
    vsync();
    pass_position();
    put_draw_env(u32(at(draw_environment)));
    put_disp_env(u32(at(draw_environment)) + display_environment);
    move_image(u32(at(screen_images)) + 0x1180, 0, static_cast<std::int32_t>(shown * 0xe0));
    draw_otag(u32(at(draw_environment)) + sent_table);
    poll_cards();
    list_unscanned_ports();
}

// 801c7d78: decode the frame's input into menu state + 325. A missing
// controller (80035734(0) zero) waits for one with the sound paused
// (80037ee4, 80037e8c); an overflowed input queue (80036410) is reset
// (80035db0) and reads as 8 (none). Otherwise the queue is drained until
// an entry has one of the menu buttons: pressed 2000, 4000, 8000, 1000
// (0-3, with sound 1), 8005948c bits 20 (4, sound 2), 40 (5, sound 3), 80
// (6), 10 (7), pressed 4 (a, sound 1), 8 (9, sound 1) or 100 (c).
void Overlay::decode_input() {
    const auto stack_frame = enter(0x20);
    if (pad_kind(0) == 0)
        missing("menu_input", 0x801c7dac, "symbol:controller-wait-80037ee4",
                "Waiting for a controller with the sound paused (80037ee4, 80037e8c) is not "
                "recovered");
    std::uint32_t code = 8;
    auto &queue = program.resident.input_queue;
    if (queue.overflow != 0) { // 80036410
        queue.reset();         // 80035db0
    } else {
        while (queue.dequeue()) { // 80035cdc
            const auto pressed = u16(pressed_buttons);
            const auto other = u16(pressed_buttons_2);
            std::uint32_t sound = 0;
            if ((pressed & 0x2000U) != 0) {
                code = 0;
                sound = 1;
            } else if ((pressed & 0x4000U) != 0) {
                code = 1;
                sound = 1;
            } else if ((pressed & 0x8000U) != 0) {
                code = 2;
                sound = 1;
            } else if ((pressed & 0x1000U) != 0) {
                code = 3;
                sound = 1;
            } else if ((other & 0x20U) != 0) {
                code = 4;
                sound = 2;
            } else if ((other & 0x40U) != 0) {
                code = 5;
                sound = 3;
            } else if ((other & 0x80U) != 0) {
                code = 6;
            } else if ((other & 0x10U) != 0) {
                code = 7;
            } else if ((pressed & 4U) != 0) {
                code = 0xa;
                sound = 1;
            } else if ((pressed & 8U) != 0) {
                code = 9;
                sound = 1;
            } else if ((other & 0x100U) != 0) {
                code = 0xc;
                break;
            } else {
                continue;
            }
            if (sound != 0)
                play_menu_sound(sound);
            break;
        }
    }
    put8(at(input_code), code);
}

// 801c7f34: split the play counter (VSyncs at 60 per second) into menu state
// words 2ec..304: hundreds of hours, tens of hours, hours, tens of minutes,
// minutes, tens of seconds and seconds.
void Overlay::split_play_time(std::uint32_t a0) {
    const auto frames = a0;
    const auto state_block = state();
    auto rest = frames % 21600000U; // 100 hours
    put32(state_block + 0x2ec, frames / 21600000U);
    put32(state_block + 0x2f0, rest / 2160000U); // 10 hours
    rest %= 2160000U;
    put32(state_block + 0x2f4, rest / 216000U); // 1 hour
    rest %= 216000U;
    put32(state_block + 0x2f8, rest / 36000U); // 10 minutes
    rest %= 36000U;
    put32(state_block + 0x2fc, rest / 3600U); // 1 minute
    rest %= 3600U;
    put32(state_block + 0x300, rest / 600U); // 10 seconds
    put32(state_block + 0x304, rest % 600U / 60U);
}

// 801d1d40: the 3D view. Motion 3 starts a zoom from distance 800 (motion
// 1), 4 from 200 (motion 2), both from angle 0; motion 1 pulls in by 30 and
// turns x by 7c until below 200 (then rests at 200, angles 0); motion 2
// pushes out by 40 and turns y by -60 until e00. The rotation (8003f738)
// and translation (80049d9c) become the view matrix (+1f0), loaded into the
// geometry coprocessor (80049efc, 80049f8c).
void Overlay::update_view() {
    const auto stack_frame = enter(0x18);
    const auto s = state();
    const auto motion = u8(s + view_motion);
    if (motion == 2) {
        const auto distance = u32(s + view_distance) + 0x40U;
        put32(s + view_distance, distance);
        put16(s + view_angles + 2, u16(s + view_angles + 2) - 0x60U);
        if (static_cast<std::int32_t>(distance) >= 0xe00)
            put8(s + view_motion, 0);
    } else if (motion == 1) {
        const auto distance = u32(s + view_distance) - 0x30U;
        put32(s + view_distance, distance);
        put16(s + view_angles, u16(s + view_angles) + 0x7cU);
        if (static_cast<std::int32_t>(distance) < 0x200) {
            put32(s + view_distance, 0x200);
            put16(s + view_angles + 4, 0);
            put16(s + view_angles, 0);
            put16(s + view_angles + 2, 0);
            put8(s + view_motion, 0);
        }
    } else if (motion == 3 || motion == 4) {
        put32(s + view_distance, motion == 3 ? 0x800U : 0x200U);
        put16(s + view_angles + 4, 0);
        put16(s + view_angles + 2, 0);
        put16(s + view_angles, 0);
        put32(s + view_offset + 4, 0);
        put32(s + view_offset, 0);
        put8(s + view_motion, motion == 3 ? 1U : 2U);
    }
    const auto base = state();
    const auto matrix = base + view_matrix;
    field::GteMatrix m{};
    for (std::uint32_t i = 0; i < 9; ++i)
        m.r[i] = static_cast<std::int16_t>(s16(matrix + 2 * i));
    const field::GteVector angles{static_cast<std::int16_t>(s16(base + view_angles)),
                                  static_cast<std::int16_t>(s16(base + view_angles + 2)),
                                  static_cast<std::int16_t>(s16(base + view_angles + 4))};
    m = field::rotation_matrix(angles, program.resident.math.trigonometry, m); // 8003f738
    for (std::uint32_t i = 0; i < 9; ++i)
        put16(matrix + 2 * i, static_cast<std::uint16_t>(m.r[i]));
    for (std::uint32_t i = 0; i < 3; ++i) // 80049d9c TransMatrix
        put32(matrix + 0x14 + 4 * i, u32(base + view_offset + 4 * i));
    auto &gte = program.resident.gte.transform;
    for (std::uint32_t i = 0; i < 9; ++i) // 80049efc SetRotMatrix
        gte.r[i] = static_cast<std::int16_t>(s16(matrix + 2 * i));
    for (std::uint32_t i = 0; i < 3; ++i) // 80049f8c SetTransMatrix
        gte.t[i] = s32(matrix + 0x14 + 4 * i);
}

// 801d2968: with party +6 set, the status panel (801d5cf8(d0, ca)).
void Overlay::draw_status_panel() {
    const auto stack_frame = enter(0x18);
    if (u8(u32(at(party)) + 6) != 0)
        draw_play_time(0xd0, 0xca);
}

// 801d1ca0: while drawing (+327), the screen of the menu mode (0 801d1b20,
// 2 801d1be8, 6 801d1c48), then the frame primitives (801d1258).
void Overlay::draw_screen() {
    const auto stack_frame = enter(0x18);
    if (u8(at(drawing)) != 0) {
        const auto mode = u8(menu_mode);
        if (mode == 0)
            draw_field_menu_screen();
        else if (mode == 2)
            draw_title_file_screen();
        else if (mode == 6)
            missing("menu_draw", 0x801d1d20, "symbol:menu-801d1c48",
                    "The mode 6 screen (801d1c48) is not recovered");
    }
    link_fade();
}

// 801d1258: link this buffer's fade quad (primitive block + 98 + 18 * index)
// and draw mode (+140 + c * index) into the top table (+90).
void Overlay::link_fade() {
    const auto stack_frame = enter(0x18);
    const auto index = u32(at(buffer_index));
    add_prim(u32(at(draw_environment)) + top_table, u32(at(primitives)) + 0x98 + index * 0x18);
    add_prim(u32(at(draw_environment)) + top_table, u32(at(primitives)) + 0x140 + index * 0xc);
}

// 801d1e80: start the zoom-in (motion 3) with sound 5b.
void Overlay::zoom_in() {
    const auto stack_frame = enter(0x18);
    put8(at(view_motion), 3);
    play_menu_sound(0x5b);
}

// 801d1eb0: start the zoom-out (motion 4) with sound 5c.
void Overlay::zoom_out() {
    const auto stack_frame = enter(0x18);
    put8(at(view_motion), 4);
    play_menu_sound(0x5c);
}

// 801d22c4: hide the cursors (party +4, +3).
void Overlay::hide_cursors() {
    put8(u32(at(party)) + 4, 0);
    put8(u32(at(party)) + 3, 0);
}

// 801d22f4(kind): the markers of the marker block (+428): party +2f cleared;
// kind 0 sets it and +144/+145 and, like kind 2, builds the four marker
// quads of sheet entry 108 at 801e9a58/801e9a68 (8002675c, scale 800); kind
// 3 builds one at (0, 0). Each records the buffer index (+148 + i).
void Overlay::set_markers(std::uint32_t a0) {
    const auto stack_frame = enter(0x30);
    const auto kind = low8(a0);
    put8(u32(at(party)) + 0x2f, 0);
    if (kind == 1 || kind > 3)
        return;
    if (kind == 3) {
        static_cast<void>(resident::sheet_quads(*this, u32(at(sheet)), 0x108, u32(at(markers)),
                                                u32(at(buffer_index)), 0, 0, 0x800));
        put8(u32(at(markers)) + 0x148, u8(at(buffer_index)));
        return;
    }
    if (kind == 0) {
        put8(u32(at(party)) + 0x2f, 1);
        put8(u32(at(markers)) + 0x144, 1);
        put8(u32(at(markers)) + 0x145, 1);
    }
    for (std::uint32_t i = 0; i < 4; ++i) {
        static_cast<void>(resident::sheet_quads(
            *this, u32(at(sheet)), 0x108, u32(at(markers)) + 0x50 * i, u32(at(buffer_index)),
            u32(0x801e9a58 + 4 * i), u32(0x801e9a68 + 4 * i), 0x800));
        put8(u32(at(markers)) + 0x148 + i, u8(at(buffer_index)));
    }
}

// 801d2484: clear party +2f (the markers of 801d22f4).
void Overlay::clear_markers() { put8(u32(at(party)) + 0x2f, 0); }

// ---- Top-level loops ------------------------------------------------------------

// 801d2d38: open the field menu. On first opening (mode 0) the two portrait
// blocks (+364.., 720 bytes) and their marks (+380.., 18 bytes) with their
// portraits (801e53cc), and sound 5e. Each party member's name label and
// gear label (+a0 + b, or ff) (801e8da8), the command labels (801e8474 over
// 801ea19c), the party panels (801d29a8(1, 0)) and the status (801d28fc).
void Overlay::open_field_menu() {
    const auto stack_frame = enter(0x20);
    if (u8(menu_mode) == 0) {
        for (std::uint32_t i = 0; i < 2; ++i) {
            const auto portrait = allocate(0x720, 0, 0x801d2d60);
            put32(at(portraits) + 4 * i, portrait);
            static_cast<void>(bzero(portrait, 0x720));
            const auto marks = allocate(0x18, 0, 0x801d2d8c);
            put32(at(portrait_marks) + 4 * i, marks);
            static_cast<void>(bzero(marks, 0x18));
            prepare_window_packets(i);
        }
        play_menu_sound(0x5e);
    }
    for (std::uint32_t slot = 0, row = 6; slot < 3; ++slot, row += 2) {
        const auto id = u8(u32(at(party)) + party_ids + slot);
        if (id == 0xff)
            continue;
        load_character_name(id, (slot * 2) & 0xfeU);
        const auto gear =
            u8(character_records + u8(u32(at(party)) + party_ids + slot) * 0xa4 + character_gear);
        load_character_name(gear == 0xff ? 0xffU : low8(gear + 0xb), row & 0xffU);
    }
    reveal_sprite_columns(8, 0x801ea19c);
    slide_party_panels(1, 0);
    open_play_time_window();
}

// 801c55a0: the field menu's command loop. Up/down (codes 1 and 3) move the
// cursor over the seven commands with wrap; confirm (4) runs the command
// (801c531c(0)) unless it is command 2 with no gear pilot (sound 4); cancel
// (5) leaves. A moved cursor redraws the command labels (801e8978,
// 801e8070).
void Overlay::field_menu_loop() {
    const auto stack_frame = enter(0x30);
    for (bool running = true; running;) {
        menu_frame();
        const auto code = u8(at(input_code));
        if (code == 3) {
            put8(at(cursor), u8(at(cursor)) + 1);
            if (u8(at(cursor)) >= 7)
                put8(at(cursor), 0);
        } else if (code == 1) {
            put8(at(cursor), u8(at(cursor)) == 0 ? 6U : u8(at(cursor)) - 1U);
        } else if (code == 4) {
            bool allowed = true;
            if (u8(at(cursor)) == 2 && u8(at(fighters)) == 0) {
                allowed = false;
                play_menu_sound(4);
            }
            if (allowed) {
                put8(u32(at(screen_images)) + 0x1192, 1);
                hide_cursors();
                clear_bytes(8, u32(at(party)) + 0xc);
                running = low8(run_command(0)) != 0;
            }
        } else if (code == 5) {
            running = false;
        }
        if (u8(at(cursor)) != u8(at(cursor_shown))) {
            build_sprite_columns(7, u8(at(cursor)), 0x801ea19c);
            place_label(8, at(command_labels), 0x801ea528, 0x801e9e64, u32(at(party)) + 0xc,
                        u8(at(cursor)), 0, 0);
            put8(at(cursor_shown), u8(at(cursor)));
        }
    }
}

// 801c58ec: the title file screen's command loop over three commands (the
// file commands 7..9 of 801c531c), as 801c55a0; the frame counter restarts on
// each input, and on disc 1 after 600 idle frames the screen leaves with
// 800594d0 = 1 (the title's timeout).
void Overlay::title_file_loop() {
    const auto stack_frame = enter(0x30);
    std::uint32_t running = 1;
    reveal_sprite_columns(4, 0x801ea1d4);
    layout_labels_row4(8, at(command_labels), 0x801ea530);
    put32(at(frame_counter), 0);
    for (;;) {
        menu_frame();
        const auto code = u8(at(input_code));
        if (code == 3) {
            put8(at(cursor), u8(at(cursor)) + 1);
            if (u8(at(cursor)) >= 3)
                put8(at(cursor), 0);
            put32(at(frame_counter), 0);
        } else if (code == 1) {
            put8(at(cursor), u8(at(cursor)) == 0 ? 2U : u8(at(cursor)) - 1U);
            put32(at(frame_counter), 0);
        } else if (code == 4) {
            put8(u32(at(screen_images)) + 0x1192, 1);
            hide_cursors();
            clear_bytes(8, u32(at(party)) + 0xc);
            put8(u32(at(primitives)) + 0x15b, 0x40);
            running = run_command(7);
            put8(reset_enabled, 0);
            put32(at(frame_counter), 0);
        }
        if (u8(at(cursor)) != u8(at(cursor_shown))) {
            build_sprite_columns(3, u8(at(cursor)), 0x801ea1d4);
            place_label(8, at(command_labels), 0x801ea530, 0x801e9e84, u32(at(party)) + 0xc,
                        u8(at(cursor)), 0, 0);
            put8(at(cursor_shown), u8(at(cursor)));
        }
        if (current_disc() == 1 && u32(at(frame_counter)) > 600) {
            running = 0;
            put8(load_resume, 1);
        }
        if (low8(running) == 0)
            break;
    }
    clear_bytes(8, u32(at(party)) + 0xc);
}

// 801c531c(offset): run command cursor + offset. Commands: 0 none, 1 the
// file screen for saving (801d9f98(0, 801e96a4)), 2 801e23cc, 3 801de29c
// (+4dc, 1), 4 items (801dbe54), 5 equipment (801e0f78(+4dc, 1)), 6
// 801e2be4, 7 sound mode (801d9808), 8 the file screen for loading
// (801d9f98(1, 0); a load sets 800594d0 = 2 and ends the menu), 9 new game
// data (8001b970, ends the menu). A command that returns nonzero zooms back
// out (801d1eb0) and redraws the menu (field menu 801d29a8(1, 0); title the
// labels and line color 4c). Then 801e3088(offset), 801d3674, the screen
// block flags (+1192 0, +1193 1), the cursors on, the labels redrawn and
// the reset check enabled. Returns 0 when the menu should end.
std::uint32_t Overlay::run_command(std::uint32_t a0) {
    const auto stack_frame = enter(0x20);
    std::uint32_t keep_menu = 1;
    std::uint32_t result = 1;
    put8(in_command, 1);
    const auto command = u8(at(cursor)) + low8(a0);
    if (command < 10) {
        switch (command) { // the jump table at 801c5000
        case 1:
            result = file_screen(0, u8(save_file_screen));
            break;
        case 2:
            missing("menu_command", 0x801c53d0, "symbol:menu-801e23cc",
                    "Menu command 2 (801e23cc) is not recovered");
        case 3:
            missing("menu_command", 0x801c53f0, "symbol:menu-801de29c",
                    "Menu command 3 (801de29c) is not recovered");
        case 4:
            result = item_screen();
            break;
        case 5:
            result = run_equipment_screen(u8(at(first_member)), 1);
            break;
        case 6:
            missing("menu_command", 0x801c5430, "symbol:menu-801e2be4",
                    "Menu command 6 (801e2be4) is not recovered");
        case 7:
            result = sound_mode_screen();
            break;
        case 8:
            if (low8(file_screen(1, 0)) != 0) {
                put8(load_resume, 2);
                keep_menu = 0;
            }
            break;
        case 9:
            load_new_game_data(); // 8001b970
            [[fallthrough]];
        default: // 0
            result = 0;
            keep_menu = 0;
            break;
        }
    }
    put8(u32(at(card_state)) + card_mode, 0);
    if (low8(result) != 0) {
        zoom_out();
        const auto mode = u8(menu_mode);
        if (mode == 0) {
            slide_party_panels(1, 0);
        } else if (mode == 2) {
            layout_labels_row4(8, at(command_labels), 0x801ea530);
            put8(u32(at(primitives)) + 0x15b, 0x4c);
        }
    }
    leave_menu_screen(low8(a0));
    release_help_block();
    put8(u32(at(screen_images)) + 0x1192, 0);
    put8(u32(at(screen_images)) + 0x1193, 1);
    put8(u32(at(party)) + 4, 1);
    put8(u32(at(party)) + 3, 1);
    put8(at(cursor_shown), 0xff);
    put8(u32(at(party)) + 10, 0);
    put8(reset_enabled, 1);
    return keep_menu;
}

// Resident 8001b970, the menu's New Game: read the new-game data (file 3 of
// directory 10) over the game data (8006d634, 2358 bytes) through a block
// allocated with owner tag 2 (80032498(2, 0)), turn each of the 31 stored
// names (20 bytes of two-byte codes, ending at code 000f) back into text
// (80033b34) and store the 20 bytes the decoder's buffer holds, then clear
// 20 halfwords from 8005a3c6 down to the saved globals (8005a3a0), set the
// field menu cursor (800594cc) to 6 and clear 8005947c.
void Overlay::load_new_game_data() {
    const auto stack_frame = enter(0x58);
    const auto locals = frame(0x58);
    const auto codes = locals[0x10];
    const auto text = locals[0x28];
    static_cast<void>(select_directory(0x10, 0));
    auto &heap = program.resident.heap;
    heap.tag = 2; // 80032498(2, 0)
    heap.tag_words[2] = 0;
    heap.quiet = 0;
    const auto block = allocate(file_words(3), 1, 0x8001b9ac);
    static_cast<void>(read_file(3, block, 0, 0x80));
    disc_wait(0);
    constexpr std::uint32_t game = 0x8006d634;
    static_cast<void>(memmove(game, block, 0x2358));
    release(block, 0x8001b9ec);
    for (std::uint32_t name = 0; name < 0x26c; name += 0x14) {
        const auto stored = game + name;
        std::uint32_t length = 0;
        for (; length < 0x14; length += 2) {
            put8(codes + length, u8(stored + length));
            put8(codes + length + 1, u8(stored + length + 1));
            if (u8(stored + length) == 0x0f && u8(stored + length + 1) == 0)
                break;
        }
        decode_text(codes, text, length / 2); // 80033b34
        for (std::uint32_t i = 0; i < 0x14; ++i)
            put8(stored + i, u8(text + i));
    }
    // Twenty halfwords downward from 8005a3c6: the sound driver's master
    // volume pair (8005a3c4) and commit word (8005a3c0, both halves), then
    // the saved globals 8005a3a0..8005a3bf.
    put16(0x8005a3c6, 0);
    put16(0x8005a3c4, 0);
    put32(0x8005a3c0, 0);
    for (std::uint32_t i = 16; i-- > 0;)
        put16(0x8005a3a0 + 2 * i, 0);
    put8(0x800594cc, 6);
    put8(0x8005947c, 0);
}

// 801c8694(disc): after the zoom (801d1e80) settles, the markers (801d22f4(0));
// while the current disc (80028530) is not `disc` + 1 the disc-change prompt
// runs (801e92cc, 801d2f4c, 801e93a0; not recovered). Then 801d2484.
void Overlay::check_loaded_disc(std::uint32_t a0) {
    const auto stack_frame = enter(0x28);
    zoom_in();
    while (u8(at(view_motion)) != 0)
        menu_frame();
    set_markers(0);
    const auto wanted = low8(a0) + 1U;
    if (current_disc() != wanted)
        missing("menu_disc_check", 0x801c8728, "symbol:menu-disc-change-801e92cc",
                "The disc change prompt (801e92cc, 801e93a0) is not recovered");
    clear_markers();
}

// 801d9808: the sound mode screen. The choices (labels 801ea574 into
// +1be0, 801e86c8(0)) start at the choice of the driver's output mode
// (80038824: flags 8005957c & 700 clear 0, & 600 set 2, else 1; mode 0 is
// choice 0, 1 choice 2, 2 choice 1); up/down wrap over +33a choices; confirm
// (4) applies the chosen mode (800386c4, the same mapping back) and cancel
// (5) leaves. Returns 1.
std::uint32_t Overlay::sound_mode_screen() {
    const auto stack_frame = enter(0x38);
    const auto choice_of = [](std::uint32_t value, std::uint32_t previous) {
        return value == 0 ? 0U : value == 1 ? 2U : value == 2 ? 1U : previous;
    };
    bool first = true;
    bool running = true;
    bool apply = false;
    std::uint32_t mode = 0;
    do {
        menu_frame();
        if (first) {
            layout_labels_row4(4, at(sound_labels), 0x801ea574);
            reveal_row_list(0);
            put8(at(choice_shown), 0xff);
            const auto flags = program.resident.sound.flags; // 80038824
            const auto current = (flags & 0x700U) == 0 ? 0U : (flags & 0x600U) != 0 ? 2U : 1U;
            mode = choice_of(current, mode);
            first = false;
            put8(at(choice), mode);
        }
        if (u8(at(choice)) != u8(at(choice_shown))) {
            place_label(6, at(sound_labels), 0x801ea578, 0x801e9f88, u32(at(party)) + 0x5c,
                        u8(at(choice)), 7, 0);
            build_row_list(0);
            put8(at(choice_shown), u8(at(choice)));
        }
        const auto code = u8(at(input_code));
        if (code == 3) {
            put8(at(choice), u8(at(choice)) + 1);
            if (u8(at(choice)) >= u8(at(choice_count)))
                put8(at(choice), 0);
        } else if (code == 1) {
            put8(at(choice),
                 u8(at(choice)) == 0 ? u8(at(choice_count)) + 0xffU : u8(at(choice)) - 1U);
        } else if (code == 4) {
            apply = true;
            running = false;
        } else if (code == 5) {
            running = false;
        }
    } while (running);
    if (apply) {
        mode = choice_of(u8(at(choice)), mode);
        program.set_sound_mode(static_cast<std::int32_t>(mode)); // 800386c4
    }
    clear_bytes(4, u32(at(party)) + 0x5c);
    put8(u32(at(party)) + 4, 0);
    put8(u32(at(party)) + 3, 0);
    return 1;
}

// 801d9f34: the file screen's labels (801e8018 into +de0; 801ea542 on the
// title, else 801ea53c).
void Overlay::draw_file_labels() {
    const auto stack_frame = enter(0x18);
    layout_labels_row4(6, at(file_labels), u8(menu_mode) == 2 ? 0x801ea542U : 0x801ea53cU);
}

// 801d9f98(loading, screen): the file screen. On its first pass it draws
// the card panels (801e6450, 801e5acc), marks both ports present, zooms in
// and shows the header (801e86c8: 7 on the title, 9 from the field menu
// unless opened by triangle), then checks the cards (801d9c84); a card
// problem ends it (with result 0 unless in the field menu). Each frame the
// card notice (801d2f4c(20) / 801d32b4 around 801c93a8's check; a nonzero
// check ends it), the choice labels (801e8070, 801e8b4c), and the input:
// up/down wrap over +33a choices, confirm (4) runs the file action
// (801cd710(screen)) and on the title continues, else ends; cancel (5)
// ends. With `loading` set, ending by confirm or cancel returns 0. Without
// cards on the title the result is 0. Returns 1 otherwise.
std::uint32_t Overlay::file_screen(std::uint32_t a0, std::uint32_t a1) {
    const auto stack_frame = enter(0x48);
    std::uint32_t running = 1;
    std::uint32_t result = 1;
    bool first = true;
    std::uint32_t header = 0;
    put8(reset_enabled, 0);
    put8(at(choice), 2);
    if (u8(menu_mode) == 0 && u8(triangle_menu) == 0)
        put8(at(choice), 1);
    put8(at(choice_shown), 0xff);
    draw_file_labels();
    menu_frame();
    do {
        put8(u32(at(party)) + 0xb, 0);
        put8(at(load_state), 1);
        if (first) {
            build_file_select_panels();
            build_file_slots();
            put8(u32(at(card_state)) + card_ports, 1);
            put8(u32(at(card_state)) + card_ports + 1, 1);
            zoom_in();
            const auto mode = u8(menu_mode);
            if (mode == 2) {
                header = 7;
            } else if (mode == 0) {
                slide_party_panels(0, 0);
                if (u8(triangle_menu) == 0)
                    header = 9;
            }
            reveal_row_list(header);
            put8(u32(at(party)) + 6, 0);
            first = false;
            put8(u32(at(party)) + 0x21, 0);
            if (low8(enter_card_mode()) != 0) {
                if (u8(menu_mode) != 0)
                    result = 0;
                break;
            }
            put8(u32(at(party)) + 0x68, 1);
        }
        if (u8(card_message) != 0)
            show_message(0x20);
        if (low8(refresh_cards()) != 0)
            running = 0;
        if (u8(card_message) != 0) {
            close_message();
            put8(card_message, 0);
        }
        if (low8(running) == 0)
            break;
        if (u8(at(choice)) != u8(at(choice_shown))) {
            place_label(6, at(file_labels), 0x801ea542, 0x801e9ea0, u32(at(party)) + 0x1a,
                        u8(at(choice)), 7, 0);
            build_row_list(u8(menu_mode) == 2 ? 7U : 0U);
            put8(at(choice_shown), u8(at(choice)));
        }
        const auto code = u8(at(input_code));
        if (code == 3) {
            put8(at(choice), u8(at(choice)) + 1);
            if (u8(at(choice)) >= u8(at(choice_count)))
                put8(at(choice), 0);
        } else if (code == 1) {
            put8(at(choice),
                 u8(at(choice)) == 0 ? u8(at(choice_count)) + 0xffU : u8(at(choice)) - 1U);
        } else if (code == 4 || code == 5) {
            bool leave = true;
            if (code == 4) {
                hide_cursors();
                clear_bytes(6, u32(at(party)) + 0x1a);
                running = run_file_command(low8(a1));
                put8(at(choice_shown), 0xff);
                draw_file_labels();
                leave = u8(menu_mode) != 2;
            }
            if (leave) {
                running = 0;
                if (low8(a0) != 0)
                    result = 0;
            }
        }
    } while (low8(running) != 0);
    if (u16(u32(at(card_state)) + card_ports) == 0 && u8(menu_mode) != 0)
        result = 0;
    put8(u32(at(party)) + 0x68, 0);
    put8(u32(at(party)) + 0xb, 0);
    put8(u32(at(party)) + 4, 0);
    put8(u32(at(party)) + 3, 0);
    clear_bytes(6, u32(at(party)) + 0x1a);
    close_card_events();
    return result;
}

} // namespace xem::reconstruction::menu
