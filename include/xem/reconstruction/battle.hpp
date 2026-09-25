#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <stdexcept>
#include <vector>

namespace xem::reconstruction {
struct ResidentState;
}

namespace xem::reconstruction::battle {

class BattleError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

// Battle-mode memory: the battle overlay (sha256 1830b4ef...) image loaded at
// 8006faf0 with its static data up to 800d39f0 (resident mode table row
// 800180ac), plus the heap blocks battle setup allocates. Each region is owned
// whole and addressed by original address, as the original code addresses it.
struct BattleMemory {
    std::map<std::uint32_t, std::vector<std::uint8_t>> regions;

    [[nodiscard]] std::uint32_t u8(std::uint32_t address) const;
    [[nodiscard]] std::uint32_t u16(std::uint32_t address) const;
    [[nodiscard]] std::uint32_t u32(std::uint32_t address) const;
    [[nodiscard]] std::int32_t s8(std::uint32_t address) const;
    [[nodiscard]] std::int32_t s16(std::uint32_t address) const;
    void put8(std::uint32_t address, std::uint32_t value);
    void put16(std::uint32_t address, std::uint32_t value);
    void put32(std::uint32_t address, std::uint32_t value);
    // The bytes from `address` to the end of its region.
    [[nodiscard]] std::span<const std::uint8_t> tail(std::uint32_t address) const;
    // Move [address, address + size) out of its region, splitting the region
    // (a disc read list becomes the reader's while its read runs).
    std::vector<std::uint8_t> take(std::uint32_t address, std::uint32_t size);
};

inline constexpr std::uint32_t overlay_base = 0x8006faf0;
inline constexpr std::uint32_t overlay_end = 0x800d39f0;

// Combatant records: 11 slots (0-2 party, 3-10 enemies) of 0x170 bytes from the
// base pointer at 800c34b0.
inline constexpr std::uint32_t record_base_pointer = 0x800c34b0;
inline constexpr std::uint32_t record_stride = 0x170;
// Heap block of turn and menu state allocated by battle setup.
inline constexpr std::uint32_t turn_state_pointer = 0x800c3eac;
inline constexpr std::uint32_t combatant_slots = 11;

// Resolver globals.
inline constexpr std::uint32_t descriptor_pointer = 0x800c3dfc;     // current command descriptor
inline constexpr std::uint32_t attacker_pointer = 0x800c3e00;       // attacker record
inline constexpr std::uint32_t attacker_slot = 0x800c3e04;          // u8
inline constexpr std::uint32_t target_pointer = 0x800c3e34;         // target record
inline constexpr std::uint32_t target_slot = 0x800c3e50;            // u8
inline constexpr std::uint32_t target_tail_pointer = 0x800c3d60;    // target record + 148
inline constexpr std::uint32_t attacker_block_pointer = 0x800d2d6c; // attacker record + a4
inline constexpr std::uint32_t target_block_pointer = 0x800d2dc8;   // target record + a4

// A battle computation over battle memory and the resident rand state
// (8005a1fc).
struct Battle {
    BattleMemory &memory;
    std::uint32_t &seed;

    [[nodiscard]] std::uint32_t record_base() const { return memory.u32(record_base_pointer); }
    [[nodiscard]] std::uint32_t record(std::uint32_t slot) const {
        return record_base() + slot * record_stride;
    }
    // Resident rand 8003fa38: 0..7fff.
    std::uint32_t rand();
};

// 80085ccc: commit an action (attacker slot, target slot mask, animation)
// and resolve it (800941a4).
void commit_action(Battle &battle, std::uint32_t attacker, std::uint32_t targets,
                   std::uint32_t animation);

// Formula functions of the table at 800c348c, called with the resolver
// globals set for one target.
void physical_formula(Battle &battle); // 80094ee4 (type 0)
void formula_type3(Battle &battle);    // 80095d4c (type 3)

// Per-target steps the resolver calls around the formula.
void post_adjust(Battle &battle);         // 800946f4
void counter_check(Battle &battle);       // 800968c0
void ether_check(Battle &battle);         // 80096824
void status_effect_958d8(Battle &battle); // 800958d8
void status_effect_95a78(Battle &battle); // 80095a78
void status_effect_95b44(Battle &battle); // 80095b44

// 80085618: apply queued results (event slot `queue & 0xff`, 0x48 bytes from
// 800c3fe8: u16 amounts, u8 codes at +0x18) to HP, EP, gear HP and record
// +0xdc, marking knockouts.
void apply_results(Battle &battle, std::uint32_t queue);
// 800883ac: drop a slot from its formation group (group byte 800c3eb4,
// member byte 800c3eb5, 4-byte group entries at 800d301c; enemies use entries
// 8.., slots with 800d32a1[slot*8] set add 0x10).
void leave_group(Battle &battle, std::uint32_t slot);
// 8007252c: rebuild the alive mask (800d39dc) and set the battle outcome
// (800c48ea) when either side is defeated.
void update_alive(Battle &battle);

// 800799c8: run enemy `slot`'s AI script (the 0x40-byte block at
// 800d3400 + (slot - 3) * 0x40), filling the action list at 800d2e5c.
void run_enemy_script(Battle &battle, std::uint32_t slot, std::uint32_t flag);

// 8007171c: one ATB tick for every present slot that is not ready.
void atb_tick(Battle &battle);
// 800718bc: reload the acting slot's turn timer (80098af8).
void reload_turn_timer(Battle &battle);

// The turn procedure between its presentation calls (frames, animation,
// text). 80070f40 calls 800723e0 every frame while the battle runs.
//
// 800723e0 up to its call of 80071b94, plus that procedure's entry up to its
// actor branch: take the forced slot (800d2dc0) or the next ready slot in the
// turn order from the cursor 800d2dd7. Returns whether a slot acts.
bool select_turn(Battle &battle);
// 80071bd8..80071c74: start the selected slot's turn (turn state +2d3 becomes
// the slot) and count down its timed statuses (80099890).
void begin_turn(Battle &battle);
void count_down_statuses(Battle &battle, std::uint32_t slot);
// 80071c74 up to the actor's branch: a party turn clears the reaction flags
// 800c3d1a and places the acting member's marker. Returns the branch: 0 enemy
// script (80071c80), 1 command menu 80080160 (80072090), 2 menu 80080c94
// (800720a0, +80 bit 2000), 3 no menu (800720f8, +7c bits 2080 or +80 bit 1000).
std::uint32_t prepare_turn(Battle &battle);
// 80079778 up to its first frame: reset the event queue for `actor`.
void begin_actions(Battle &battle, std::uint32_t actor);
// Presentation call of the action executor: 800bc404 frames the camera on a
// slot mask (camera state and GTE matrices only).
using FrameTargets = std::function<void(std::uint32_t mask)>;
// 800793f0 and 80079674: execute the action list 800d2e5c (approach, commit,
// results, presentation events) and close the event queue.
void execute_actions(Battle &battle, std::uint32_t actor, const FrameTargets &frame);
// The same from the return of a type-2 entry's camera call (80078c6c) at
// action `index`; `more` is the executor's pending type-0 flag.
void resume_actions(Battle &battle, std::uint32_t actor, std::uint32_t index, bool more,
                    const FrameTargets &frame);
// 80072160..800721dc: party timers held by 800d2c9e and every slot's
// default target (800841e0).
void order_targets(Battle &battle);
// 800721ec..80072230: alive update and end-of-turn regeneration check.
void settle_turn(Battle &battle);
// 80072240..80072254: reload the turn timer and re-enable the ATB.
void finish_turn(Battle &battle);

// The party command menu 80080160: every frame the battle tick decodes pad
// input into the command code 800d3014 and the menu dispatches its current
// page (turn state +2dd) with that code.
//
// 80089ccc: decode the resident input queue (800594a4 buttons: 2000/4000/
// 8000/1000 -> codes 0-3; 8005948c keys: 20/40/80/10 -> 4-7, 100 -> d,
// 800 -> e; 8 when empty, ff once the battle ends or events finish), playing
// the menu effects.
void decode_input(Battle &battle, ResidentState &resident);
// 80080160 dispatch (table 8006fe7c): one frame of page +2dd for `member`.
// Pages 1, 3 and 9 are reconstructed: face codes move between pages (a
// repeated press of the same button on some items), 4/6/7 confirm: page 3
// defends, page 9 tries to escape.
void menu_step(Battle &battle, ResidentState &resident, std::uint32_t member);
// 8009aa44: the Defense command for `member`.
void defend(Battle &battle, std::uint32_t member);
// 8009a9d0: an escape attempt; rand % 100 < 50 succeeds and writes the party
// back (8009be0c). The caller sets the outcome 800c48ea = 40.
bool try_escape(Battle &battle, std::uint32_t slot);
// 8009be0c: write HP, EP, use counters and gear HP back to the persistent
// character (8006d8a0) and gear (8006dfac) records.
void write_back_party(Battle &battle);

// Presentation calls of the attack pages. The reconstruction does not run
// them: `present` observes each at its call. The attack model spans frames
// and runs the battle's logic tick, as does a frame, so a step ends there and
// a resume entry continues after it.
enum class MenuPresentation : std::uint8_t {
    camera,        // 800bc404: frame the camera on a slot mask
    target_camera, // 800bcd98: point the target camera at a slot mask
    target_text,   // 80093b08: the target name window (member)
    attack_model,  // 800b89fc: load and pose the member's attack model (member)
    model_reset,   // 800b8da4: return the attack model to its stance
    combo_camera,  // 800bcd98(0) in 800819a4: the target cursor leaves
    frame,         // 800716d8: one battle frame
};
using MenuPresent = std::function<void(MenuPresentation, std::uint32_t argument)>;
// menu_step with the attack pages: page 1 enters the attack page (80087a38,
// 80084a7c, 80077698) up to the attack model; page 5 (80081b58) targets by
// direction (8008189c, 80084854), confirms attacks and, once the member's AP
// pay for them, executes the combo (800819a4, 800861d0, 80087af0) up to the
// frame inside 800861d0; page 0x64 (80080838) resets +2e2 and runs page 5,
// which stops at once while +2e1 is set.
void menu_step(Battle &battle, ResidentState &resident, std::uint32_t member,
               const MenuPresent &present);
// Page 1's confirmation of the attack item (800811b8): enter page 5.
void enter_attack_page(Battle &battle, ResidentState &resident, std::uint32_t member,
                       const MenuPresent &present);
// 80081b58: one frame of the attack page.
void attack_page(Battle &battle, ResidentState &resident, std::uint32_t member,
                 const MenuPresent &present);
// 80081b98 (from the target text's return, 80094134): the attack page after
// 8008189c, with its input.
void resume_attack_view(Battle &battle, ResidentState &resident, std::uint32_t member,
                        const MenuPresent &present);
// 800819e4: the combo start after its target camera call, then the rest of
// the confirmation (80081ee0).
void resume_combo(Battle &battle, ResidentState &resident, std::uint32_t member,
                  const MenuPresent &present);
// 80081ee0: the confirmation after 800819a4, up to the frame in 800861d0.
void confirm_attack(Battle &battle, ResidentState &resident, std::uint32_t member,
                    const MenuPresent &present);
// 80087ac0: the attack page entry after the attack model returns.
void resume_attack_entry(Battle &battle, ResidentState &resident, std::uint32_t member);
// 80086b34: the attack confirmation after the frame inside 800861d0: release
// the three text blocks, execute the attack (80087af0: group change, combo
// step, commit, reaction script 80079ab0, results) and reload the member's
// turn timer by the AP left.
void resume_attack_confirm(Battle &battle, ResidentState &resident, std::uint32_t member);
// 80084854: the candidate (800c3e90) nearest to `origin` in screen direction
// `direction` (0-3), measured by ratan2 over the slot positions; `origin`
// when none lies that way.
std::uint32_t direction_target(const Battle &battle, const ResidentState &resident,
                               std::uint32_t origin, std::uint32_t direction);

// 80076a10 (8002675c at scale 1): build sprite `id` of the glyph table
// (800d2f5c) as POLY_FT4 primitives at `destination` (0x50 per part, 0x28 per
// draw buffer 800ccb34) placed at x, y; returns its part count.
std::uint32_t draw_glyph(Battle &battle, std::uint32_t id, std::uint32_t destination,
                         std::uint32_t x, std::uint32_t y);

// Turn-procedure helpers the attack pages share.
void approach_route(Battle &battle, std::uint32_t actor, std::uint32_t target);    // 800877e0
void join_target_group(Battle &battle, std::uint32_t actor, std::uint32_t target); // 80087edc
void clear_current_event(Battle &battle);                                          // 80085388
// 80085c88: accumulate and apply event `queue`'s results.
void apply_event(Battle &battle, std::uint32_t queue);
std::uint32_t order_candidates(Battle &battle, std::uint32_t actor); // 800841e0
// 80079ab0 up to its executor call: enemy `slot`'s reaction script (pointer
// at 800d3408 + (slot - 3) * 0x40) when armed (800c3d18 + (slot - 3) * 4).
// Returns whether it ran action 62.
bool run_reaction_script(Battle &battle, std::uint32_t slot);
// 8008aa40 and 8008aa74: a menu sound effect; the latter while 800d366c
// enables menu effects.
void play_menu_effect(ResidentState &resident, std::uint32_t id);
void play_enabled_menu_effect(Battle &battle, ResidentState &resident, std::uint32_t id);

// Battle setup. 80070f40 copies the formation record (8006f9dc, 0x20 bytes)
// from the field's formation table (800658dc, field component 6) at the
// selector 80059508: +0 enemy set, +1 flags, +2 scene, +4..6 party groups,
// +8..f enemy ids (7f none), +10..17 enemy flags, +18..1f enemy groups.
inline constexpr std::uint32_t formation_record = 0x8006f9dc;
inline constexpr std::uint32_t formation_table = 0x800658dc;
inline constexpr std::uint32_t formation_table_bytes = 0x210;
// The party ids 801e4048 publishes (3 bytes).
inline constexpr std::uint32_t battle_party_ids = 0x80059468;
// The setup module: directory 12 file 4 (sha256 4300fdd9...), loaded at
// 801e4000 by 8001bbac.
inline constexpr std::uint32_t setup_module_base = 0x801e4000;

// 801e5840 phase 1: participants (801e4048), formation groups and positions
// (801e4160), enemy records and AI script pointers from the enemy data file
// *800c3dd0 (801e4870) and the party's derived stats and flags (801e4ac0:
// 80097d5c, 8009b098).
void setup_participants(Battle &battle, ResidentState &resident);
// 801e5840 phase 2: items (801e4cd0), the turn order and initial turn timers
// (801e4e7c: 80078508, 80098af8, 8001bd40), the turn state's command tables
// and default targets (801e5014) and the direction-arrow block (800c3e24,
// 0xec bytes through 8008abb8).
void setup_turns(Battle &battle, ResidentState &resident);
// 80098af8: a slot's turn timer from its speed, with rand spread.
std::uint32_t turn_timer(Battle &battle, std::uint32_t slot);
// 8008abb8(size, mode): 80032498(2, 0), then 80031bdc; the block becomes
// battle memory.
std::uint32_t allocate_block(Battle &battle, ResidentState &resident, std::uint32_t size,
                             std::uint32_t mode);

// Post-battle module (directory 10 file 4, sha256 f474fd48..., loaded at
// 801de000) and the persistent game data (8006d634, 2358 bytes) are
// addressable through the same memory during victory processing.
inline constexpr std::uint32_t result_module_base = 0x801de000;
inline constexpr std::uint32_t result_module_bytes = 0x6500;

// 801e2794: victory rewards: experience split and level-ups, skills, flags,
// write-back to persistent party state and drop rolls.
void grant_rewards(Battle &battle);
// 801e2280 up to its UI setup call at 801e23d4: the experience pool
// (800d2c84), defeated mask (800d2c9c) and gold (8006ef58, capped at 9999999)
// from knocked-out enemies.
void total_rewards(Battle &battle);
// 801e1444: add up to 8 drops (id, count and category arrays) to the
// inventory lists (801e1370).
void add_drops(Battle &battle, std::uint32_t ids, std::uint32_t counts, std::uint32_t categories);
// 801e2acc: split the experience pool and apply level-ups (801e308c).
void distribute_experience(Battle &battle);

// The result screens (801e1fb8) between frames: continue after the frame
// that returned to `site` (the frame call's return address) up to the next
// frame call and return that call's return address. The Cross waits of the
// summary, experience, level-up and gold/items screens, the flags the
// screens raise and clear (UI +a0, +a1, +ac, +b0..b2, +cf) and the window
// closes (8008fa60, releasing the blocks of window `window`, S0 there).
// Screen contents (text, windows, sounds, the skill and item lists) stop
// with a BattleError.
std::uint32_t result_screen_step(Battle &battle, ResidentState &resident, std::uint32_t site,
                                 std::uint32_t window);

} // namespace xem::reconstruction::battle
