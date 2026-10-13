/*
 * The Binaural sound mode's sources (docs/runtime.md, Binaural sound mode):
 * the host is told which sprite runs a script command that plays a sound,
 * and every effect the sound driver starts (its first effect channel and
 * id), as they happen, so it can place a sound a sprite's animation plays
 * where that sprite is. The calls only tell: the game, the driver and the
 * SPU run as the original. Like the rest of the port, nothing here keeps
 * data of its own (a port global would move the port's stack, whose
 * addresses the game keeps in RAM).
 */
#include "common.h"

/* The sprite running a sound command (0 when it ends), and an effect start:
 * `code`'s low byte the first effect channel, `id` the bank << 16 | effect. */
void xem_host_spu_sound_source(u32 sprite);
void xem_host_spu_effect_started(u32 code, u32 id);

void xem_original_sound_start_effect(s16 code, s32 id, s16 volume, s16 pan);
void xem_original_sprite_vm_run_generic_command(void *sprite, u8 op, u8 *code);
void xem_original_battle_sprite_command_run(void *sprite, s32 command, u8 *args);

void sound_start_effect(s16 code, s32 id, s16 volume, s16 pan) {
    xem_original_sound_start_effect(code, id, volume, pan);
    xem_host_spu_effect_started(code, id);
}

/* The sprite script commands that play sounds: b9 (the sprite's own bank)
 * and b0 (the scripts' bank). */
void sprite_vm_run_generic_command(void *sprite, u8 op, u8 *code) {
    if (op != 0xB9 && op != 0xB0) {
        xem_original_sprite_vm_run_generic_command(sprite, op, code);
        return;
    }
    xem_host_spu_sound_source((u32)sprite);
    xem_original_sprite_vm_run_generic_command(sprite, op, code);
    xem_host_spu_sound_source(0);
}

/* The battle sprite commands that play sounds: 4f and 52 (the sprite's
 * bank) and 56 (a Gear slot's sound). */
void battle_sprite_command_run(void *sprite, s32 command, u8 *args) {
    if (command != 0x4F && command != 0x52 && command != 0x56) {
        xem_original_battle_sprite_command_run(sprite, command, args);
        return;
    }
    xem_host_spu_sound_source((u32)sprite);
    xem_original_battle_sprite_command_run(sprite, command, args);
    xem_host_spu_sound_source(0);
}
