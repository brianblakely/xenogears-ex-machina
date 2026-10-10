/* The callees and game data port/adapters.c uses, as test doubles that
 * record each call's arguments for tests/test_port_adapters.py. Nothing here
 * is original content. */
#include "common.h"
#include "psyq/libgte.h"
#include "resident/sprite.h"
#include "battle/frame.h"

BattleMenu *battle_current_menu;
s32 sprite_palette_bank;

static BattleMenu menu;
static Sprite sprites[3];
static s32 recorded[8];
static s32 calls;

static void record(s32 count, s32 a, s32 b, s32 c, s32 d, s32 e) {
    s32 values[5];
    s32 i;

    values[0] = a;
    values[1] = b;
    values[2] = c;
    values[3] = d;
    values[4] = e;
    for (i = 0; i < 8; i++) {
        recorded[i] = i < count ? values[i] : -1;
    }
    calls++;
}

/* The acting sprite becomes sprites[slot]; the previous one is returned
 * from nothing (the original is void). */
void battle_menu_set_acting_slot(s32 slot) {
    record(1, slot, 0, 0, 0, 0);
    battle_current_menu->slot = slot;
    battle_current_menu->sprite = &sprites[slot];
}

void battle_seal_deathblow_commands(u8 member, u8 checked) {
    record(2, member, checked, 0, 0, 0);
}

void battle_sprite_arrive_at_target(Sprite *sprite, Sprite *other) {
    record(2, (s32)sprite, (s32)other, 0, 0, 0);
}

s32 field_actor_update_position(s32 index, s32 lowest, void *descriptor, void *actor, s32 status) {
    record(5, index, lowest, (s32)descriptor, (s32)actor, status);
    return -1;
}

void gear_model_step_and_draw(MATRIX *m, void *light, u32 *ot, s32 buffer, s32 elapsed) {
    record(5, (s32)m, (s32)light, (s32)ot, buffer, elapsed);
}

/* Records the palette bank in force during the call as a sixth value. */
Sprite *sprite_create(s32 *data, s16 clut_x, s16 clut_y, s16 texture_x, s16 texture_y, s16 unused) {
    record(5, (s32)data, clut_x, clut_y, texture_x, texture_y);
    recorded[5] = unused;
    recorded[6] = sprite_palette_bank;
    return &sprites[2];
}

void test_reset(void) {
    s32 i;

    battle_current_menu = &menu;
    menu.sprite = 0;
    menu.slot = -1;
    for (i = 0; i < 3; i++) {
        sprites[i].partner = &sprites[(i + 1) % 3];
    }
    sprite_palette_bank = 0;
    calls = 0;
}

s32 test_recorded(s32 i) { return recorded[i]; }
s32 test_calls(void) { return calls; }
s32 test_sprite(s32 i) { return (s32)&sprites[i]; }
s32 test_palette_bank(void) { return sprite_palette_bank; }
