/*
 * Calls whose types differ from the callee's definition (tools/game_module.py,
 * build/game/adapters.txt) where the generated adapter's 0 for a missing
 * argument or a void result would change behaviour. Each xem_adapt_<function>
 * takes the call's types and supplies what the PS1 left in the register or
 * stack word, read from the matched code (addresses are the original's).
 *
 * Reviewed and left to the generated adapter, because the callee never reads
 * the missing value or the caller ignores the result:
 * - heap_report_printf (heap.c): the one-argument calls pass formats without
 *   conversions, so sprintf never reads `args`.
 * - battle_put_item_in_character4_entry (battle_command_submenus.c, call at
 *   8008be50): $a1 holds the cell index row * 2 + column (8008bd80), but the
 *   call is guarded by battle_is_character4_entry_item(item - 50), which
 *   tests the same id byte (8009a800, 22558 + 16 * (item - 50) = 21758 +
 *   16 * item, 8009a878) against the four entries that override `k`
 *   (8009a888-8009a8d0); gear part ids are 50-72 (battle_setup_phases.c).
 * - sprite_get_render_kind (sprite_construction.c, 80023c08, 80024024):
 *   `fallback` is read for kind 3 only. $a1 holds the child's `header`
 *   (sprite_create_child) or `source` (sprite_create_effect), a pointer that
 *   would send sprite_task_create down its no-renderer path through an unset
 *   $s0 (80023a84-80023a8c, 80023b4c-80023b54). A child's kind 3 is replaced
 *   by its parent's type, which is never 3 (80023bf0-80023c04; types are 0,
 *   4, 14 or a child's resolved kind); an effect with a kind 3 header would
 *   corrupt memory on the PS1, so shipped effects have none.
 * - functions given more arguments than they declare (the callee never
 *   reads an argument it does not declare) and void functions whose callers
 *   discard the result (console_empty_debug_hook, field_set_quad_uvs_clamped,
 *   menu_detail_layout_tabs, model_set_tpage_override, text_decode_codes,
 *   worldmap_build_translucent_quads).
 */
#include "common.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"
#include "battle/frame.h"

void battle_menu_set_acting_slot(s32 slot);
void battle_seal_deathblow_commands(u8 member, u8 checked);
void battle_sprite_arrive_at_target(Sprite *sprite, Sprite *other);
s32 field_actor_update_position(s32 index, s32 lowest, void *descriptor, void *actor, s32 status);
void gear_model_step_and_draw(MATRIX *m, void *light, u32 *ot, s32 buffer, s32 elapsed);
Sprite *sprite_create(s32 *data, s16 clut_x, s16 clut_y, s16 texture_x, s16 texture_y, s16 unused);

/* battle_flow.c uses the acting sprite the void 800beff4 leaves in $v0: it
 * loads BATTLE_AREA.sprites[slot] at 800bf094 and stores it to
 * battle_current_menu->sprite at 800bf09c, the function's last write. */
Sprite *xem_adapt_battle_menu_set_acting_slot(s32 slot) {
    battle_menu_set_acting_slot(slot);
    return battle_current_menu->sprite;
}

/* battle_resolve_gear_action (8009c198) passes no `checked`, which 8009ac48
 * only tests for zero. $a1 at the call (8009c23c) is battle_work_ptr
 * (0x800ccce8, loaded at 8009c1b8) for an attacker slot below 3; otherwise
 * it is the caller's $a1, which battle_resolve_action sets to battle_work_ptr
 * (800941b8) before its first call (80094244) and which the preceding
 * battle_seal_deathblow_commands(slot, 1) leaves at 1 or battle_work_ptr
 * (8009ac48, 8009acd8) before its second (800942e8). Its low byte is
 * nonzero in every case. */
void xem_adapt_battle_seal_deathblow_commands(s32 member) {
    battle_seal_deathblow_commands(member, 1);
}

/* battle_walk_beside_target (800bf4f0) calls 800b9c00 at 800bf5a4 without
 * `other`; $a1 still holds its own `target` argument (never written in
 * 800bf4f0), which battle_sprites_face_each_other turns the sprite toward.
 * battle_walk_next_path_point (800bf118-800bf12c) and battle_menu_update's
 * pending action 6 (800ba1b4-800ba1b8) pass sprite->partner; its state-6
 * walk (800ba47c) passes $s3, the menu's target read on entry (800b9fb8),
 * which battle_face_first_target
 * sets together with sprite->partner (800bf3e8) but which a later partner
 * change (sprite command 3e) can leave behind. sprite->partner is exact for
 * the first two and for the third while the two agree. */
void xem_adapt_battle_sprite_arrive_at_target(Sprite *sprite) {
    battle_sprite_arrive_at_target(sprite, sprite->partner);
}

/* field_party_place_at_controlled (80077268) calls 80084a40 without the
 * stack argument `status`, which 80084a40 reads from 16(sp) of the caller's
 * 48-byte frame (80084a40 never writes it; 80077268 saves registers from
 * 24(sp)). The word is what earlier calls left 32 bytes below their
 * caller's stack pointer:
 * - field_load_from_bundle (80071968): field_layer_start's saved $ra, the
 *   return address 0x80077c78 into field_layer_load_and_start (80077c70:
 *   field_layer_start's 88-byte frame under field_layer_load_and_start's
 *   24-byte one saves $ra at 80(sp)); field_reload_actor_blocks (56-byte
 *   frame, saves from 40(sp)) and field_instance_refresh_bounds_modes
 *   (48-byte frame, saves from 24(sp)), called after it, leave it.
 * - field_party_run_join_event (8008bb00): field_actor_reset's saved $s0
 *   (160-byte frame, 128(sp)), the index of the joining actor's descriptor
 *   (8008ba94); field_event_run_instructions writes the word only through
 *   console_printf's argument spill on the development error path.
 * The adapter cannot tell the callers apart and passes the map load's
 * value: nonzero and not below 2 (80084a40 tests `(u32)status < 2` with
 * the ground override, else `status != 0`), as the join event's is unless
 * the joining actor is descriptor 0 (or 1 with the ground override). */
s32 xem_adapt_field_actor_update_position(s32 index, s32 lowest, void *descriptor, void *actor) {
    return field_actor_update_position(index, lowest, descriptor, actor, 0x80077C78);
}

/* gear_shop_draw_model (801ce7e0) calls 801e7d14 without `elapsed`, which
 * 801e7d14 reads from 16(sp) of the caller's frame (801e7d20: 88(sp) under
 * its own 72 bytes), where 801ce7ec saved $ra: the return address 801cb480
 * of its only call (801cb478). The scene then advances 0 and 3 steps on
 * alternate frames: the pending half-frames wrap. */
void xem_adapt_gear_model_step_and_draw(MATRIX *m, void *light, u32 *ot, s32 buffer) {
    gear_model_step_and_draw(m, light, ot, buffer, 0x801CB480);
}

/* field.c stores the sprite the void 80024294 leaves in $v0: sprite_create's
 * result (800242d8), which nothing after it overwrites (800242e0-800242ec).
 * The body is 80024294's, keeping that result. */
Sprite *xem_adapt_sprite_create_with_palette_bank(void *data, s16 clut_x, s16 clut_y, s16 texture_x, s16 texture_y,
                                                  s32 unused, s32 extra) {
    Sprite *sprite;

    sprite_palette_bank = extra;
    sprite = sprite_create(data, clut_x, clut_y, texture_x, texture_y, unused);
    sprite_palette_bank = 0;
    return sprite;
}
