/* ovl3087 (Disc 1 slot 3087 / Disc 2 slot 3082), loaded at 801e5000: the
 * battle event script interpreter. The battle overlay loads it (80070e2c,
 * file list entry 1 through 800295d8) only when the formation sets
 * 800c3d48 (formation flag 0x20), then calls 801e5160 once to load the
 * script files and set up the threads, 801e879c (through 80070eb0, at the
 * start and between turns) to run the script threads until opcode 22 hands
 * back, and 801e563c at the end to release everything. Opcode
 * handlers use the battle overlay's actor, camera and message services
 * (8007xxxx-800cxxxx) and resident file/heap/sound helpers.
 *
 * This unit holds the overlay's rodata (801e5000), the interpreter and its
 * opcode handlers (801e5160-801e93e8) and the overlay's data (801e9b5c); it
 * ends at 801e93e8, where the actor helpers' compiler takes over
 * (script_actor.c, ovl3087.mk). */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libgpu.h"
#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/text.h"
#include "resident/window.h"
#include "battle/actions.h"
#include "battle/actor.h"
#include "battle/area.h"
#include "battle/combatant.h"
#include "battle/event_script.h"
#include "battle/flow.h"
#include "battle/formation.h"
#include "battle/frame.h"
#include "battle/graphics.h"
#include "battle/input.h"
#include "battle/objects.h"
#include "battle/scene.h"
#include "battle/setup.h"
#include "battle/turn.h"
#include "battle/ui.h"
#include "battle/windows.h"
#include "battle/work.h"
#include "ovl3087.h"

/* Portrait file per actor, normal then mirrored (file 0x46 + n). */
u8 battle_event_script_portrait_files[] = { /* 801E9B5C */
    0,  0,  6,  6,  17, 17, 19, 20, 21, 21, 23, 23, 24, 24, 28, 28,
    27, 27, 17, 17, 25, 25, 34, 34, 35, 35, 36, 36, 37, 37, 79, 79,
    82, 82, 83, 83, 26, 26, 52, 52, 81, 81, 77, 77, 78, 78, 33, 33,
    41, 41, 29, 29, 43, 43, 50, 51, 42, 42, 53, 53, 56, 56, 38, 38,
    1,  1,  2,  2,  3,  3,  4,  4,  5,  5,  7,  7,  8,  8,  9,  9,
    10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 18, 18,
    30, 30, 31, 31, 32, 32, 39, 39, 40, 40, 44, 44, 45, 45, 46, 46,
    47, 47, 48, 48, 49, 49, 54, 54, 22, 22, 57, 57, 58, 58, 59, 59,
    60, 60, 61, 61, 62, 62, 63, 63, 64, 64, 65, 65, 66, 66, 67, 67,
    68, 68, 69, 69, 70, 70, 71, 71, 72, 72, 73, 73, 74, 74, 75, 75,
    76, 76, 80, 80, 55, 55, 84, 84, 85, 85, 86, 86, 87, 87, 88, 88,
    89, 89, 90, 90,
};
/* Default message window layout. */
u16 battle_event_script_default_window_layout[5] = {0x7FFF, 0x7FFF, 16, 8, 0x1F0}; /* 801E9C10 */
s32 battle_event_script_cursor_frame = 4; /* 801E9C1C: the cursor glyph's frame, cycled 4..0 */
u8 battle_event_script_actions_started[16] = {0}; /* 801E9C20 */
s32 battle_event_script_text_origin_x = 0; /* 801E9C30 */
s32 battle_event_script_text_origin_y = 0; /* 801E9C34 */
ModelArchive *battle_event_script_model_archive = NULL; /* 801E9C38 */

/* The actor and model helpers (script_actor.c) as this unit declares them:
 * its calls convert the actor, animation and target arguments differently
 * from the definitions (bytes and halfwords where those take words). */
void battle_event_script_play_model_animation(s32 model, u16 animation);
s32 battle_event_script_create_model(void *file, s32 *position);
void battle_event_script_start_actor_animation(u8 actor, s16 animation);
void battle_event_script_return_actor_to_idle(u8 actor);
void battle_event_script_stop_actor_commands(u8 actor);
void battle_event_script_clear_actor_countdown(u8 actor);
void battle_event_script_start_actor_move(u8 actor, s16 x, s16 y, s16 z);
void battle_event_script_start_actor_animation3(u8 actor, s16 x, s16 y, s16 z);
void battle_event_script_give_actor_command(u8 actor, u16 command);
void battle_event_script_run_actor_attack(u8 actor, u8 target);
void battle_event_script_turn_actor_to_target(u8 actor, u16 target);
void battle_event_script_destroy_model(s32 model);
void battle_event_script_reset_camera(void);

/* 801E5160: Load the script set of 8006f9df (script archive file 2) and the model
 * archive (file 3), set up the interpreter state and its threads, the
 * portrait quads and the script's sound bank (file 4). */
void battle_event_script_load(void) {
    FileRequest files[3];
    ScriptArchive *archive;
    EventScriptFile *script;
    s32 i;
    s32 level;

    battle_wait_frame();
    battle_cd_select_event_script_directory();
    archive = battle_heap_alloc(cd_get_aligned_file_size(2), 1);
    files[0].file = 2;
    files[0].destination = archive;
    battle_event_script_model_archive = battle_heap_alloc(cd_get_aligned_file_size(3), 0);
    files[1].file = 3;
    files[1].destination = battle_event_script_model_archive;
    files[2].file = 0;
    files[2].destination = NULL;
    cd_read_file_list(files, 0, 0x80);
    battle_cd_wait_for_reads();
    text_relocate_offset_table(archive);
    text_relocate_offset_table(battle_event_script_model_archive);
    script = text_unpack_lzss_alloc(((ScriptSet *)((u8 *)archive + formation_active.scriptSet * 8))->script, 0);
    battle_messages_of_event_script = text_unpack_lzss_alloc(((ScriptSet *)((u8 *)archive + formation_active.scriptSet * 8))->data, 0);
    heap_free(archive);
    battle_state_of_event_script = battle_heap_alloc(sizeof(ScriptState), 0);
    bzero((u8 *)battle_state_of_event_script, sizeof(ScriptState));
    battle_message_text_window = battle_heap_alloc(0x98, 0);
    bzero((u8 *)battle_message_text_window, 0x78);
    battle_file_of_event_script = script;
    battle_state_of_event_script->code = (u8 *)battle_file_of_event_script + battle_file_of_event_script->threadCount * 16 + 0x44;
    for (i = 0; i < 16; i++) {
        battle_state_of_event_script->order[i] = 0xFF;
    }
    for (i = 0; i < battle_file_of_event_script->threadCount; i++) {
        for (level = 0; level < 8; level++) {
            battle_state_of_event_script->threads[i].pc[level] = 0xFFFF;
            battle_state_of_event_script->threads[i].priority[level] = 0xFF;
            battle_state_of_event_script->threads[i].entry[level] = 0xFF;
        }
        battle_state_of_event_script->order[i] = i;
        battle_state_of_event_script->threads[i].pc[0] = battle_file_of_event_script->entries[i].entry[0];
        battle_state_of_event_script->threads[i].priority[0] = 0;
        battle_state_of_event_script->threads[i].order = 0xFF;
        battle_state_of_event_script->threads[i].entry[0] = 0;
        battle_state_of_event_script->threads[i].level = 0;
        battle_state_of_event_script->threads[i].request = 0xFF;
        battle_state_of_event_script->threads[i].speaker = 0xFF;
    }
    battle_state_of_event_script->unk7F5 = 4;
    for (i = 0; i < 5; i++) {
        battle_state_of_event_script->window[i] = battle_event_script_default_window_layout[i];
    }
    for (i = 0; i < 2; i++) {
        SetPolyFT4(&battle_state_of_event_script->quads[i]);
        setRGB0(&battle_state_of_event_script->quads[i], 0x80, 0x80, 0x80);
        SetSemiTrans(&battle_state_of_event_script->quads[i], 0);
        SetShadeTex(&battle_state_of_event_script->quads[i], 1);
        battle_state_of_event_script->quads[i].clut = GetClut(0, 0x1D0);
        battle_state_of_event_script->quads[i].tpage = GetTPage(1, 0, 0x3C0, 0x100);
    }
    battle_ui->scriptLoaded = 1;
    battle_ui->waitingCross = 0;
    for (i = 0; i < 16; i++) {
        battle_state_of_event_script->actionRunning[i] = 0;
        battle_event_script_actions_started[i] = 0;
    }
    battle_state_of_event_script->musicPlaying = 0;
    battle_cd_select_event_script_directory();
    battle_state_of_event_script->soundBank = battle_heap_alloc(cd_get_aligned_file_size(4), 0);
    cd_read_file(4, battle_state_of_event_script->soundBank, 0, 0x80);
    battle_cd_wait_for_reads();
    sound_add_effect_bank(battle_state_of_event_script->soundBank);
    sound_sync_transfer(0x10);
    battle_state_of_event_script->soundBankLoaded = 1;
    battle_sound_bank_of_event_script = battle_state_of_event_script->soundBank;
    battle_load_wave_bank_5();
}

/* 801E563C: Release the interpreter state, the script and model files, stop the
 * music and release the sound bank. Returns whether music was playing. */
s32 battle_event_script_release(void) {
    s32 musicWasPlaying;

    heap_free(battle_state_of_event_script);
    musicWasPlaying = 0;
    heap_free(battle_message_text_window);
    heap_free(battle_file_of_event_script);
    heap_free(battle_messages_of_event_script);
    heap_free(battle_event_script_model_archive);
    if (battle_state_of_event_script->musicPlaying != 0) {
        musicWasPlaying = 1;
        sound_stop_seq((SoundSeq *)battle_music_seq);
        battle_wait_frame();
        sound_release_seq((SoundSeq *)battle_music_seq);
        battle_wait_frame();
    }
    if (battle_state_of_event_script->soundBankLoaded != 0) {
        sound_stop_bank_effects(battle_state_of_event_script->soundBank);
        sound_remove_effect_bank(battle_state_of_event_script->soundBank);
        battle_wait_frame();
        heap_free(battle_state_of_event_script->soundBank);
        battle_state_of_event_script->soundBankLoaded = 0;
    }
    return musicWasPlaying;
}

/* 801E5768: Make the thread's highest occupied level current; return its pc. */
u16 battle_event_script_select_top_level(ScriptThread *thread) {
    s32 i;
    s32 level;
    u32 free;

    i = 0;
    free = 0xFF;
    for (; i < 8; i++) {
        if (thread->priority[i] < free) {
            level = i;
        }
    }
    thread->level = level;
    thread->runningEntry = thread->entry[level];
    return thread->pc[level];
}

/* 801E57C4: First free level above the base level (8 when none is free). */
u16 battle_event_script_find_free_level(ScriptThread *thread) {
    s32 i;

    for (i = 1; i < 8; i++) {
        if (thread->priority[i] == 0xFF) {
            break;
        }
    }
    return i;
}

/* 801E57F8: Decode count 16-bit operands following the opcode into the operand
 * slots. Each is an immediate when its bit (from 0x80 down) is set in
 * immediateMask, otherwise a variable offset. In the signed form bit 15
 * marks an immediate instead. */
void battle_event_script_decode_operands(u8 *insn, u8 count, u8 immediateMask, u8 signedForm) {
    s32 i;
    s16 raw;

    for (i = 0; i < count; i++) {
        if (signedForm) {
            raw = insn[i * 2 + 1] + (insn[i * 2 + 2] << 8);
            if (raw & 0x8000) {
                battle_state_of_event_script->operands[i] = raw & 0x7FFF;
            } else {
                battle_state_of_event_script->operands[i] = battle_state_of_event_script->vars[(s16)(raw / 2)];
            }
        } else if ((immediateMask << i) & 0x80) {
            battle_state_of_event_script->operands[i] = insn[i * 2 + 1] + (insn[i * 2 + 2] << 8);
        } else {
            battle_state_of_event_script->operands[i] =
                SCRIPT_VAR(battle_state_of_event_script, (insn[i * 2 + 2] << 8) | insn[i * 2 + 1]);
        }
    }
}

/* 801E58EC: Compare two script values with condition op (low four bits). */
u8 battle_event_script_compare(s16 a, s16 b, u8 op) {
    s32 result = 0;

    switch (op & 0xF) {
    case 0:
        if (a == b) {
            result = 1;
        }
        break;
    case 1:
        if (a != b) {
            result = 1;
        }
        break;
    case 2:
        if (a > b) {
            result = 1;
        }
        break;
    case 3:
        if (a < b) {
            result = 1;
        }
        break;
    case 4:
        if (a >= b) {
            result = 1;
        }
        break;
    case 5:
        if (a <= b) {
            result = 1;
        }
        break;
    case 6:
        if (a & b) {
            result = 1;
        }
        break;
    case 7:
        if (a != b) {
            result = 1;
        }
        break;
    case 8:
        if (a | b) {
            result = 1;
        }
        break;
    case 9:
        if (battle_is_slot_in_mask((u16)a, (u8)b)) {
            result = 1;
        }
        break;
    case 10:
        if (!battle_is_slot_in_mask((u16)a, (u8)b)) {
            result = 1;
        }
        break;
    }
    return result;
}

/* 801E5A98: Battle slot of a script actor id: party characters (ids below 16) by
 * their position in the party, enemies as id - 13. */
u8 battle_event_script_get_actor_slot(s32 id) {
    s32 slot = 0;
    s32 i;

    if ((u8)id < 16) {
        for (i = 0; i < 3; i++) {
            if (battle_party_character_ids[i] != 0xFF && battle_party_character_ids[i] == (u8)id) {
                slot = i;
                break;
            }
        }
    } else {
        slot = id - 13;
    }
    return slot;
}

/* 801E5B00: Show the next frame of the five-frame cursor glyph (glyphs 0xe0-0xe4, the
 * battle graphics' cursor quads) at (x, y), mirrored horizontally by
 * swapping the current buffer's second and third vertices. */
void battle_event_script_show_cursor_glyph(x, y)
s16 x;
s16 y;
{
    BattleGraphics *graphics;
    s32 x1;
    s32 y1;

    if (--battle_event_script_cursor_frame < 0) {
        battle_event_script_cursor_frame = 4;
    }
    battle_ui->cursorParts =
        battle_build_glyph(battle_event_script_cursor_frame + 0xE0, battle_graphics->cursor, x, y);
    graphics = battle_graphics;
    x1 = graphics->cursor[battle_drawing_state.buffer].x1;
    y1 = graphics->cursor[battle_drawing_state.buffer].y1;
    graphics->cursor[battle_drawing_state.buffer].x1 = graphics->cursor[battle_drawing_state.buffer].x2;
    graphics->cursor[battle_drawing_state.buffer].y1 = graphics->cursor[battle_drawing_state.buffer].y2;
    graphics->cursor[battle_drawing_state.buffer].x2 = x1;
    graphics->cursor[battle_drawing_state.buffer].y2 = y1;
    battle_ui->cursorBuffer = battle_drawing_state.buffer;
    battle_ui->cursorShown = 1;
}

/* 801E5C1C: Opcode 00 (end, 1 byte): drop the running level and restart the thread's
 * base level at its idle entry (1). Yields. */
s32 battle_event_script_end(s32 thread) {
    battle_state_of_event_script->threads[thread].entry[battle_state_of_event_script->threads[thread].level] = 0xFF;
    battle_state_of_event_script->threads[thread].priority[battle_state_of_event_script->threads[thread].level] = 0xFF;
    battle_state_of_event_script->threads[thread].pc[battle_state_of_event_script->threads[thread].level] = 0xFFFF;
    battle_state_of_event_script->threads[thread].entry[0] = 1;
    battle_state_of_event_script->threads[thread].level = 0;
    battle_state_of_event_script->threads[thread].priority[0] = 7;
    battle_state_of_event_script->threads[thread].pc[0] = battle_file_of_event_script->entries[thread].entry[1];
    battle_state_of_event_script->threads[thread].request = 0xFF;
    return 0;
}

/* 801E5CE4: Opcode 01 (jump, 3 bytes): continue the running level at the bytecode
 * offset (u16 at byte 1). */
s32 battle_event_script_jump(s32 thread, u8 *insn) {
    battle_state_of_event_script->threads[thread].pc[battle_state_of_event_script->threads[thread].level] = insn[1] + (insn[2] << 8);
    return 0;
}

/* 801E5D24: Opcode 02 (branch unless, 8 bytes): compare a (byte 1) and b (byte 3),
 * immediates by bits 0x80 and 0x40 of byte 5, by its low four bits
 * (801e58ec); when the comparison fails jump to the offset at byte 6. */
s32 battle_event_script_branch_unless(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 2, insn[5], 0);
    if (battle_event_script_compare(battle_state_of_event_script->operands[0], battle_state_of_event_script->operands[1], insn[5])) {
        return 8;
    }
    battle_state_of_event_script->threads[thread].pc[battle_state_of_event_script->threads[thread].level] = insn[6] + (insn[7] << 8);
    return 0;
}

/* 801E5DCC: Opcode 03 (request, 3 bytes): start entry (byte 2, low five bits) of
 * thread (byte 1) on a free level with the priority in byte 2's top three
 * bits. Retries (length 0) while that thread has no free level. */
s32 battle_event_script_request(s32 thread, u8 *insn) {
    s32 length = 0;
    u8 level = battle_event_script_find_free_level(&battle_state_of_event_script->threads[insn[1]]);

    if (level != 8) {
        battle_state_of_event_script->threads[thread].request = insn[2] & 0x1F;
        battle_state_of_event_script->threads[insn[1]].priority[level] = insn[2] >> 5;
        battle_state_of_event_script->threads[insn[1]].pc[level] =
            (battle_file_of_event_script->entries + insn[1])->entry[battle_state_of_event_script->threads[thread].request];
        length = 3;
        battle_state_of_event_script->threads[insn[1]].entry[level] = battle_state_of_event_script->threads[thread].request;
    }
    return length;
}

/* 801E5EF8: Opcode 04 (request and wait for start, 3 bytes as 03): issue the request,
 * then wait until the other thread is running the requested entry. */
s32 battle_event_script_request_wait_start(s32 thread, u8 *insn) {
    s32 length = 0;
    u8 request = battle_state_of_event_script->threads[thread].request;

    if (request != (insn[2] & 0x1F)) {
        battle_event_script_request(thread, insn);
    } else if (request == battle_state_of_event_script->threads[insn[1]].runningEntry) {
        battle_state_of_event_script->threads[thread].request = 0xFF;
        length = 3;
    }
    return length;
}

/* 801E5F8C: Opcode 05 (request and wait for end, 3 bytes as 03): issue the request,
 * then wait until the requested entry is neither queued nor running on the
 * other thread. */
s32 battle_event_script_request_wait_end(s32 thread, u8 *insn) {
    s32 length = 0;
    u8 request = battle_state_of_event_script->threads[thread].request;
    u8 finished = 1;
    s32 i;

    if (request != (insn[2] & 0x1F)) {
        battle_event_script_request(thread, insn);
    } else {
        for (i = 0; i < 8; i++) {
            if (battle_state_of_event_script->threads[insn[1]].entry[i] == request) {
                finished = 0;
                break;
            }
        }
        if (finished &&
            battle_state_of_event_script->threads[thread].request != battle_state_of_event_script->threads[insn[1]].runningEntry) {
            length = 3;
            battle_state_of_event_script->threads[thread].request = 0xFF;
        }
    }
    return length;
}

/* 801E6084: Opcode 06 (6 bytes): var (byte 1) = value (byte 3, an immediate when byte
 * 5 has bit 0x40). */
s32 battle_event_script_set(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 2, insn[5], 0);
    INSN_VAR(insn) = battle_state_of_event_script->operands[1];
    return 6;
}

/* 801E60E8: Opcode 07 (3 bytes): var (byte 1) = 1. */
s32 battle_event_script_set_one(s32 thread, u8 *insn) {
    INSN_VAR(insn) = 1;
    return 3;
}

/* 801E6118: Opcode 08 (3 bytes): var (byte 1) = 0. */
s32 battle_event_script_set_zero(s32 thread, u8 *insn) {
    INSN_VAR(insn) = 0;
    return 3;
}

/* 801E6144: Opcode 09 (6 bytes): var (byte 1) += value (byte 3, immediate by bit 0x40
 * of byte 5). */
s32 battle_event_script_add(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 2, insn[5], 0);
    INSN_VAR(insn) += battle_state_of_event_script->operands[1];
    return 6;
}

/* 801E61B4: Opcode 0a (6 bytes): var (byte 1) -= value (as 09). */
s32 battle_event_script_subtract(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 2, insn[5], 0);
    INSN_VAR(insn) -= battle_state_of_event_script->operands[1];
    return 6;
}

/* 801E6224: Opcode 0b (6 bytes): var (byte 1) |= value (as 09). */
s32 battle_event_script_or(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 2, insn[5], 0);
    INSN_VAR(insn) |= battle_state_of_event_script->operands[1];
    return 6;
}

/* 801E6294: Opcode 0c (6 bytes): var (byte 1) &= ~value (as 09). */
s32 battle_event_script_clear_bits(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 2, insn[5], 0);
    INSN_VAR(insn) &= ~battle_state_of_event_script->operands[1];
    return 6;
}

/* 801E6304: Opcode 0d (3 bytes): var (byte 1)++. */
s32 battle_event_script_increment(s32 thread, u8 *insn) {
    INSN_VAR(insn)++;
    return 3;
}

/* 801E633C: Opcode 0e (3 bytes): var (byte 1)--. */
s32 battle_event_script_decrement(s32 thread, u8 *insn) {
    INSN_VAR(insn)--;
    return 3;
}

/* 801E6374: Opcode 0f (6 bytes): var (byte 1) &= value (as 09). */
s32 battle_event_script_and(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 2, insn[5], 0);
    INSN_VAR(insn) &= battle_state_of_event_script->operands[1];
    return 6;
}

/* 801E63E4: Opcode 10 (6 bytes): var (byte 1) |= value (as 09; the same as 0b). */
s32 battle_event_script_or_10(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 2, insn[5], 0);
    INSN_VAR(insn) |= battle_state_of_event_script->operands[1];
    return 6;
}

/* 801E6454: Opcode 11 (6 bytes): var (byte 1) ^= value (as 09). */
s32 battle_event_script_xor(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 2, insn[5], 0);
    INSN_VAR(insn) ^= battle_state_of_event_script->operands[1];
    return 6;
}

/* 801E64C4: Opcode 12 (5 bytes): var (byte 1) <<= the variable at byte 3. */
s32 battle_event_script_shift_left(s32 thread, u8 *insn) {
    s32 index = ((insn[2] << 8) | insn[1]) >> 1;

    battle_event_script_decode_operands(insn, 2, 0, 0);
    battle_state_of_event_script->vars[index] <<= battle_state_of_event_script->operands[1];
    return 5;
}

/* 801E6534: Opcode 13 (5 bytes): var (byte 1) >>= the variable at byte 3. */
s32 battle_event_script_shift_right(s32 thread, u8 *insn) {
    s32 index = ((insn[2] << 8) | insn[1]) >> 1;

    battle_event_script_decode_operands(insn, 2, 0, 0);
    battle_state_of_event_script->vars[index] >>= battle_state_of_event_script->operands[1];
    return 5;
}

/* 801E65A4: Opcode 14 (3 bytes): var (byte 1) = random 0..7fff. */
s32 battle_event_script_random(s32 thread, u8 *insn) {
    INSN_VAR(insn) = battle_random_range(0, 0x7FFF);
    return 3;
}

/* 801E65FC: Opcode 15 (5 bytes): var (byte 3) = random 0..limit (u16 at byte 1). */
s32 battle_event_script_random_below(s32 thread, u8 *insn) {
    SCRIPT_VAR(battle_state_of_event_script, (insn[4] << 8) | insn[3]) = battle_random_range(0, insn[1] | (insn[2] << 8));
    return 5;
}

/* 801E6660: Opcode 16 (6 bytes): var (byte 1) = a * b, a the var operand itself (an
 * immediate by bit 0x80 of byte 5), b at byte 3 (bit 0x40). */
s32 battle_event_script_multiply(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 2, insn[5], 0);
    INSN_VAR(insn) = battle_state_of_event_script->operands[0] * battle_state_of_event_script->operands[1];
    return 6;
}

/* 801E66D8: Opcode 17 (6 bytes): var (byte 1) = a / b (signed; operands as 16). */
s32 battle_event_script_divide(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 2, insn[5], 0);
    INSN_VAR(insn) = (s32)battle_state_of_event_script->operands[0] / (s32)battle_state_of_event_script->operands[1];
    return 6;
}

/* 801E6750: Load the portrait TIM of an actor (mirrored or not) into VRAM (clut
 * 0,1d0; pixels 3c0,100) and lay the current buffer's portrait quad out
 * in a 64x64 box inside the window at (x, y). */
void battle_event_script_load_portrait(u8 actor, s32 flags, s32 x, s32 y, s32 width) {
    TIM_IMAGE tim;
    s32 mirrored = flags & 1;
    s32 file = battle_event_script_portrait_files[actor * 2 + mirrored] + 0x46;
    void *data;

    cd_select_directory(4, 0);
    data = battle_heap_alloc(cd_get_aligned_file_size(file), 1);
    cd_read_file(file, data, 0, 0x80);
    battle_cd_wait_for_reads();
    OpenTIM(data);
    ReadTIM(&tim);
    tim.crect->x = 0;
    tim.crect->y = 0x1D0;
    tim.prect->x = 0x3C0;
    tim.prect->y = 0x100;
    LoadImage(tim.crect, tim.caddr);
    LoadImage(tim.prect, tim.paddr);
    DrawSync(0);
    heap_free(data);
    if (mirrored) {
        setXY4(&battle_state_of_event_script->quads[battle_drawing_state.buffer], x + width - 4, y + 4, x + width - 0x44, y + 4,
               x + width - 4, y + 0x44, x + width - 0x44, y + 0x44);
        setUV4(&battle_state_of_event_script->quads[battle_drawing_state.buffer], 0, 0, 0x3F, 0, 0, 0x40, 0x3F, 0x40);
    } else {
        setXY4(&battle_state_of_event_script->quads[battle_drawing_state.buffer], x + 4, y + 4, x + 0x44, y + 4, x + 4, y + 0x44,
               x + 0x44, y + 0x44);
        setUV4(&battle_state_of_event_script->quads[battle_drawing_state.buffer], 0, 0, 0x40, 0, 0, 0x40, 0x40, 0x40);
    }
    battle_state_of_event_script->portraitBuffer = battle_drawing_state.buffer;
}

/* 801E6CE8: Show message of the script's message file in the layout of opcode 1a,
 * with the speaker's portrait unless flag 2 is set; returns 1 once the
 * message has been dismissed. Flags: 1 portrait left, 2 no portrait,
 * 4 lower window, 8 no window, 0x10 window style. */
u8 battle_event_script_show_message(u16 message, u8 actor, u16 flags) {
    u16 x;
    u16 y;
    u16 width;
    u16 height;
    u8 portrait;
    s32 i;
    s32 done;

    portrait = 0;
    done = 0;
    x = battle_state_of_event_script->window[0];
    y = battle_state_of_event_script->window[1];
    width = battle_state_of_event_script->window[2] * 12 + 0x18;
    if (flags == 0) {
        flags = battle_state_of_event_script->window[4];
    }
    if (battle_state_of_event_script->windowOpen == 0) {
        battle_event_script_cursor_frame = 4;
        if (battle_state_of_event_script->window[1] == 0x7FFF) {
            y = 0x10;
            if (flags & 4) {
                y = 0x8C;
            }
        }
        if (battle_state_of_event_script->window[3] >= 5) {
            height = 4;
        } else {
            height = battle_state_of_event_script->window[3];
        }
        height = height * 13 + 0x14;
        if (battle_state_of_event_script->window[0] == 0x7FFF) {
            x = 0xA0 - (width >> 1);
        }
        if (!(flags & 8)) {
            if (!(flags & 2) && actor != 0xFF) {
                width += 0x40;
                portrait = 1;
            }
            if (battle_state_of_event_script->window[0] == 0x7FFF) {
                x = 0xA0 - (width >> 1);
            }
            if (!portrait) {
                battle_window_open(0, x, y, width, height, ((flags >> 4) ^ 1) & 1, 1);
                while (battle_ui->windowOpen[0] == 0) {
                    battle_wait_frame();
                }
            } else {
                battle_event_script_load_portrait(actor, flags, x, y, width);
                battle_window_open(0, x, y, width, height, ((flags >> 4) ^ 1) & 1, 1);
                while (battle_ui->windowOpen[0] == 0) {
                    battle_wait_frame();
                }
                battle_ui->scriptPortraitShown = 1;
                if (!(flags & 1)) {
                    x += 0x40;
                }
            }
        }
        battle_event_script_text_origin_y = y + 8;
        battle_event_script_text_origin_x = x + 12;
        window_open(battle_message_text_window, 0x380, 0x100, battle_event_script_text_origin_x, battle_event_script_text_origin_y, battle_state_of_event_script->window[2] * 3,
                      battle_state_of_event_script->window[3]);
        *(u8 *)&battle_message_text_window->tile[1] = 4;
        battle_message_text_window->flags |= 2;
        window_reset_if_idle(battle_message_text_window);
        window_queue_message(battle_message_text_window, (s32)text_get_resource_entry(battle_messages_of_event_script, message));
        battle_ui->messageShown = 1;
        battle_state_of_event_script->windowOpen = 1;
        battle_wait_frame();
    }
    if (battle_message_text_window->flags & 8) {
        if (!(flags & 8)) {
            battle_event_script_show_cursor_glyph(battle_message_text_window->x * 4 + battle_event_script_text_origin_x + 2, battle_message_text_window->y * 14 + battle_event_script_text_origin_y + 5);
        }
        battle_ui->waitingCross = 1;
        if (battle_pressed_key == 4) {
            window_end_wait(battle_message_text_window);
            battle_ui->waitingCross = 0;
            battle_ui->cursorShown = 0;
        }
    }
    if (!(battle_message_text_window->flags & 4)) {
        battle_ui->messageShown = 0;
        window_close(battle_message_text_window);
        battle_wait_frame();
        battle_ui->scriptPortraitShown = 0;
        if (!(flags & 8)) {
            battle_window_close(0);
        }
        done = 1;
        battle_state_of_event_script->windowOpen = 0;
        for (i = 0; i < 5; i++) {
            battle_state_of_event_script->window[i] = battle_event_script_default_window_layout[i];
        }
    }
    return done;
}

/* 801E71D4: Opcode 18 (4 bytes): show message (u16 at byte 1) from the thread's
 * speaker with flags (byte 3; 0 takes the layout's); repeats until the
 * message is done. */
s32 battle_event_script_message(s32 thread, u8 *insn) {
    return (battle_event_script_show_message(insn[1] | (insn[2] << 8), battle_state_of_event_script->threads[thread].speaker, insn[3]) != 0) * 4;
}

/* 801E7230: Opcode 19 (5 bytes): show message (u16 at byte 2) from actor (byte 1) with
 * flags (byte 4); repeats until done. */
s32 battle_event_script_message_from(s32 thread, u8 *insn) {
    return battle_event_script_show_message(insn[2] | (insn[3] << 8), insn[1], insn[4]) ? 5 : 0;
}

/* 801E7278: Opcode 1a (11 bytes): set the message window layout from five signed-form
 * operands (x, y, width, height, flags); zero x..height take the defaults. */
s32 battle_event_script_window(s32 thread, u8 *insn) {
    s32 i;

    battle_event_script_decode_operands(insn, 5, 0, 1);
    for (i = 0; i < 4; i++) {
        if (battle_state_of_event_script->operands[i] != 0) {
            battle_state_of_event_script->window[i] = battle_state_of_event_script->operands[i];
        } else {
            battle_state_of_event_script->window[i] = battle_event_script_default_window_layout[i];
        }
    }
    battle_state_of_event_script->window[4] = battle_state_of_event_script->operands[4];
    return 11;
}

/* 801E7314: Opcode 1b (2 bytes): set the thread's speaker to actor (byte 1; f3-f5 name
 * the party members). */
s32 battle_event_script_speaker(s32 thread, u8 *insn) {
    u8 actor = insn[1];

    if (actor >= 0xF3) {
        actor = battle_party_character_ids[actor - 0xF3];
    }
    battle_state_of_event_script->threads[thread].speaker = actor;
    return 2;
}

/* 801E7358: Opcode 1c (1 byte): battle frame mode (800c3e4c) = 2. */
s32 battle_event_script_enter_script_mode(s32 thread, u8 *insn) {
    battle_frame_mode = 2;
    return 1;
}

/* 801E736C: Opcode 1d (1 byte): battle frame mode (800c3e4c) = 1. */
s32 battle_event_script_enter_turn_mode(s32 thread, u8 *insn) {
    battle_frame_mode = 1;
    return 1;
}

/* 801E7380: Opcode 1e (3 bytes): fade the screen to white over 2 * a frames (signed
 * operand a; blend mode 2, 800b39c0). */
s32 battle_event_script_fade_white(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 1, 0, 1);
    battle_screen_fade_start(battle_state_of_event_script->operands[0], 2, 0xFF, 0xFF, 0xFF);
    return 3;
}

/* 801E73D4: Opcode 1f (3 bytes): fade the screen to black over 2 * a frames (as 1e). */
s32 battle_event_script_fade_black(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 1, 0, 1);
    battle_screen_fade_start(battle_state_of_event_script->operands[0], 2, 0, 0, 0);
    return 3;
}

/* 801E7424: Opcode 49 (3 bytes): 8005942c = signed operand a. */
s32 battle_event_script_set_return_fade(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 1, 0, 1);
    mode_battle_return_fade = battle_state_of_event_script->operands[0];
    return 3;
}

/* 801E746C: Opcode 20 (1 byte): end the battle (800c3d44) and halt the script
 * (801e879c checks it when next called). */
s32 battle_event_script_halt_until_battle_end(s32 thread, u8 *insn) {
    battle_resume_event_script_at_end = 1;
    battle_state_of_event_script->halted = 1;
    return 1;
}

/* 801E748C: Opcode 21 (1 byte): 800d2d50 = 1. */
s32 battle_event_script_skip_result_screens(s32 thread, u8 *insn) {
    battle_skip_result_screens = 1;
    return 1;
}

/* 801E74A0: Opcode 22 (1 byte): make this pass of 801e879c its last, returning to the
 * battle (it repeats its passes until then). */
s32 battle_event_script_last_pass(s32 thread, u8 *insn) {
    battle_state_of_event_script->unk801 = 2;
    return 1;
}

/* 801E74B8: Opcode 37 (1 byte): request the battle exit (800d2fc4), set the outcome
 * (800c48ea) to 1 and halt the script. */
s32 battle_event_script_exit_battle(s32 thread, u8 *insn) {
    battle_exit_requested = 1;
    battle_state_of_event_script->halted = 1;
    battle_area_outcome = 1;
    return 1;
}

/* 801E74E0: Opcode 23 (7 bytes; signed operands a, b, c): object a - f3 acts on slot b
 * + 13 with its effect script c (800aa384), then waits until that effect
 * reports it is done (the effect VM's 02/03 set this thread's 0x34 through
 * 80080c6c) and finishes the battle's loads (800b8d04). */
s32 battle_event_script_act(s32 thread, u8 *insn) {
    s32 length = 0;

    battle_event_script_decode_operands(insn, 3, 0, 1);
    switch (battle_state_of_event_script->threads[battle_state_of_event_script->operands[0] - 0xF3].memberState) {
    case 0:
        battle_state_of_event_script->threads[battle_state_of_event_script->operands[0] - 0xF3].memberState = 2;
        battle_make_object_act(battle_state_of_event_script->operands[0] - 0xF3, battle_get_slot_bit(battle_state_of_event_script->operands[1] + 0xD),
                      battle_state_of_event_script->operands[2]);
        break;
    case 1:
        length = 7;
        battle_finish_loads();
        battle_load_wave_bank_5();
        battle_state_of_event_script->threads[battle_state_of_event_script->operands[0] - 0xF3].memberState = 0;
        break;
    }
    return length;
}

/* 801E75F0: Opcode 38 (7 bytes; signed operands a, b, c): object a - f3 starts its
 * effect script c on slot b + 13 (800aa320) without waiting. */
s32 battle_event_script_act_nowait(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 3, 0, 1);
    battle_start_object_script(battle_state_of_event_script->operands[0] - 0xF3, battle_get_slot_bit(battle_state_of_event_script->operands[1] + 0xD),
                  battle_state_of_event_script->operands[2]);
    return 7;
}

/* 801E7660: Opcode 4a (1 byte): put slot 0 into state 4 with timer 6 (8009c0e0(0)). */
s32 battle_event_script_set_slot0_attack_level4(s32 thread, u8 *insn) {
    battle_set_slot_attack_level4(0);
    return 1;
}

/* 801E7684: Opcode 4b (3 bytes): set bit 0 of actor a's battle record flags (0x36;
 * signed operand a). */
s32 battle_event_script_suppress_gear_hp_warning(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 1, 0, 1);
    battle_work_area.records[battle_event_script_get_actor_slot((u8)battle_state_of_event_script->operands[0])].pilot.flags36 |= 1;
    return 3;
}

/* 801E7700: Opcode 24 (5 bytes; signed operands a, b): the pending formation (8005947c)
 * = a + 1 and the battle kind (8005954c) = b: the next battle (resident
 * 8001b6c4) takes formation a of the same encounter set (80070f40). */
s32 battle_event_script_next_battle(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 2, 0, 1);
    mode_pending_battle_formation = battle_state_of_event_script->operands[0] + 1;
    mode_battle_kind = battle_state_of_event_script->operands[1];
    return 5;
}

/* 801E775C: Opcode 25 (1 byte): 800c3d5c = 1. */
s32 battle_event_script_allow_defeat(s32 thread, u8 *insn) {
    battle_defeat_allowed_by_event_script = 1;
    return 1;
}

/* 801E7770: Opcode 26 (9 bytes): store four signed operands at 8006f94e and clear the
 * state word 8004f30c (8001ac94). */
s32 battle_event_script_set_saved_map(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 4, 0, 1);
    game_data.map = battle_state_of_event_script->operands[0];
    game_data.entry[0] = battle_state_of_event_script->operands[1];
    game_data.entry[1] = battle_state_of_event_script->operands[2];
    game_data.entry[2] = battle_state_of_event_script->operands[3];
    mode_clear_field_return();
    return 9;
}

/* 801E77E4: Opcode 27 (9 bytes; signed operands a-d): 8004fe44 = a | 0x80, b, 1, c;
 * 800d3338 = 1; 80062514 = d. */
s32 battle_event_script_request_movie(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 4, 0, 1);
    (&cd_movie_request_kind)[0] = battle_state_of_event_script->operands[0] | 0x80;
    (&cd_movie_request_kind)[1] = battle_state_of_event_script->operands[1];
    (&cd_movie_request_kind)[2] = 1;
    (&cd_movie_request_kind)[3] = battle_state_of_event_script->operands[2];
    battle_continue_to_movie_mode = 1;
    cd_movie_request_last_frame = battle_state_of_event_script->operands[3];
    return 9;
}

/* 801E786C: Opcode 28 (6 bytes): fade the screen to colour (bytes 2-4) in blend mode
 * (byte 1) over 2 * (byte 5) frames (800b39c0). */
s32 battle_event_script_fade(s32 thread, u8 *insn) {
    battle_screen_fade_start(insn[5], insn[1], insn[2], insn[3], insn[4]);
    return 6;
}

/* 801E78A8: Opcode 29 (9 bytes; signed operands x, y, z, n): quake the view towards
 * amplitude (x, y, z) over 2 * n frames (800b3658). */
s32 battle_event_script_quake(s32 thread, u8 *insn) {
    u16 position[3];

    battle_event_script_decode_operands(insn, 4, 0, 1);
    position[0] = battle_state_of_event_script->operands[0];
    position[1] = battle_state_of_event_script->operands[1];
    position[2] = battle_state_of_event_script->operands[2];
    battle_quake_start(position, battle_state_of_event_script->operands[3]);
    return 9;
}

/* 801E7914: Opcode 35 (5 bytes; signed operands a, n): load model n of the model
 * archive into actor slot a (thread a + 13) unless one is loaded. */
s32 battle_event_script_load_model(s32 thread, u8 *insn) {
    s32 info[2];
    s32 slot;
    void *file;

    battle_event_script_decode_operands(insn, 2, 0, 1);
    slot = (u8)(battle_state_of_event_script->operands[0] + 13);
    if (battle_state_of_event_script->threads[slot].modelLoaded == 0) {
        file = text_unpack_lzss_alloc(battle_event_script_model_archive->entries[battle_state_of_event_script->operands[1]], 0);
        battle_state_of_event_script->threads[slot].modelFile = file;
        battle_state_of_event_script->threads[slot].model = battle_event_script_create_model(file, info);
        battle_state_of_event_script->threads[slot].modelLoaded = 1;
    }
    return 5;
}

/* 801E79E0: Opcode 2a (5 bytes; signed operands a, n): play animation n on the loaded
 * model of slot a. */
s32 battle_event_script_model_animation(s32 thread, u8 *insn) {
    s32 slot;

    battle_event_script_decode_operands(insn, 2, 0, 1);
    slot = (u8)(battle_state_of_event_script->operands[0] + 13);
    if (battle_state_of_event_script->threads[slot].modelLoaded != 0) {
        battle_event_script_play_model_animation(battle_state_of_event_script->threads[slot].model, battle_state_of_event_script->operands[1]);
    }
    return 5;
}

/* 801E7A5C: Release the slot's loaded model and its data. */
void battle_event_script_release_slot_model(s32 thread, u8 *insn) {
    s32 slot;

    battle_event_script_decode_operands(insn, 1, 0, 1);
    slot = (u8)(battle_state_of_event_script->operands[0] + 13);
    if (battle_state_of_event_script->threads[slot].modelLoaded != 0) {
        battle_event_script_destroy_model(battle_state_of_event_script->threads[slot].model);
        heap_free(battle_state_of_event_script->threads[slot].modelFile);
        battle_state_of_event_script->threads[slot].modelLoaded = 0;
    }
}

/* 801E7B08: Opcode 36 (3 bytes): release slot a's model (signed operand a). */
s32 battle_event_script_free_model(s32 thread, u8 *insn) {
    battle_event_script_release_slot_model(thread, insn);
    return 3;
}

/* 801E7B2C: Opcode 40 (3 bytes): release slot a's model and reset the script camera
 * (801e9b2c: 800c367c = 0, camera mode 0, effects disabled). */
s32 battle_event_script_free_model_camera(s32 thread, u8 *insn) {
    battle_event_script_release_slot_model(thread, insn);
    battle_event_script_reset_camera();
    return 3;
}

/* 801E7B58: Opcode 2b (3 bytes): wait until the thread's timer, set to 2 * n (signed
 * operand n), runs out. */
s32 battle_event_script_wait(s32 thread, u8 *insn) {
    s32 length = 0;

    if (battle_state_of_event_script->threads[thread].waiting == 0) {
        battle_event_script_decode_operands(insn, 1, 0, 1);
        battle_state_of_event_script->threads[thread].waitTimer = battle_state_of_event_script->operands[0] * 2;
        battle_state_of_event_script->threads[thread].waiting = 1;
    }
    if (battle_state_of_event_script->threads[thread].waitTimer == 0) {
        battle_state_of_event_script->threads[thread].waiting = 0;
        length = 3;
    }
    return length;
}

/* 801E7C0C: Opcode 2c (3 bytes): set the thread's run-order request (byte 1; byte 2
 * unused); fe moves it to the front of the run order at once. */
s32 battle_event_script_run_order(s32 thread, u8 *insn) {
    u8 order[16];
    s32 i;
    s32 next;

    battle_state_of_event_script->threads[thread].order = insn[1];
    if (insn[1] == 0xFE) {
        for (i = 0; i < 16; i++) {
            order[i] = battle_state_of_event_script->order[i];
        }
        next = 1;
        battle_state_of_event_script->order[0] = thread;
        for (i = 0; i < 16; i++) {
            if (order[i] != thread) {
                battle_state_of_event_script->order[next++] = order[i];
            }
        }
    }
    return 3;
}

/* 801E7CD0: Stop the current music and start sequence music (file music + 4) at a
 * volume. */
void battle_event_script_play_music_file(s16 music, u8 volume) {
    s32 size;

    mode_stop_music();
    if (battle_state_of_event_script->musicPlaying != 0) {
        sound_release_seq((SoundSeq *)battle_music_seq);
        battle_wait_frame();
    }
    battle_cd_select_music_directory();
    size = cd_get_aligned_file_size(music + 4);
    battle_state_of_event_script->music = mode_music_buffer;
    cd_read_file(music + 4, mode_music_buffer, 0, 0x80);
    battle_cd_wait_for_reads();
    memmove(mode_music_buffer, battle_state_of_event_script->music, size);
    battle_state_of_event_script->musicPlaying = 1;
    battle_state_of_event_script->musicId = music;
    battle_state_of_event_script->musicVolume = volume;
    battle_music_seq = sound_create_and_play_seq(mode_music_buffer, volume, 0);
}

/* 801E7DE4: Fade the music to a volume. */
void battle_event_script_fade_music_to_volume(s32 volume, s32 time) {
    sound_set_seq_fade((SoundSeq *)battle_music_seq, volume, time);
}

/* 801E7E14: Opcode 2d (3 bytes): start music a (file a + 4; signed operand) at full
 * volume. */
s32 battle_event_script_music(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 1, 0, 1);
    battle_event_script_play_music_file(battle_state_of_event_script->operands[0], 0x7F);
    return 3;
}

/* 801E7E5C: Opcode 2e (3 bytes): start music a at volume 0. */
s32 battle_event_script_music_silent(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 1, 0, 1);
    battle_event_script_play_music_file(battle_state_of_event_script->operands[0], 0);
    return 3;
}

/* 801E7EA4: Opcode 2f (5 bytes): fade the music to volume a over time b and keep a as
 * its stored volume (signed operands). */
s32 battle_event_script_music_fade(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 2, 0, 1);
    battle_state_of_event_script->musicVolume = battle_state_of_event_script->operands[0];
    battle_event_script_fade_music_to_volume(battle_state_of_event_script->operands[0], battle_state_of_event_script->operands[1]);
    return 5;
}

/* 801E7F08: Opcode 30 (3 bytes): set the music to its stored volume when a is 0, else
 * silence it. */
s32 battle_event_script_music_volume(s32 thread, u8 *insn) {
    u8 volume = 0;

    battle_event_script_decode_operands(insn, 1, 0, 1);
    if (battle_state_of_event_script->operands[0] == 0) {
        volume = battle_state_of_event_script->musicVolume;
    }
    battle_event_script_fade_music_to_volume(volume, 0);
    return 3;
}

/* 801E7F70: Opcode 31 (9 bytes; signed operands a-d): play sound a of the script bank
 * (the resident bank when d is set) with volume b and pan c (80039f18). */
s32 battle_event_script_sound(s32 thread, u8 *insn) {
    SoundBank *bank;

    battle_event_script_decode_operands(insn, 4, 0, 1);
    if (battle_state_of_event_script->operands[3] == 0) {
        bank = battle_state_of_event_script->soundBank;
    } else {
        bank = (SoundBank *)sprite_script_sound_bank;
    }
    sound_play_effect_volume_pan((bank->id << 16) | battle_state_of_event_script->operands[0], battle_state_of_event_script->operands[1],
                  battle_state_of_event_script->operands[2]);
    return 9;
}

/* 801E7FF4: Opcode 41 (7 bytes; signed operands a-c): set the volume of playing sound
 * a of the script bank (the resident one when c is set) to b (8003a2e4). */
s32 battle_event_script_sound_volume(s32 thread, u8 *insn) {
    SoundBank *bank;

    battle_event_script_decode_operands(insn, 3, 0, 1);
    if (battle_state_of_event_script->operands[2] == 0) {
        bank = battle_state_of_event_script->soundBank;
    } else {
        bank = (SoundBank *)sprite_script_sound_bank;
    }
    sound_set_effect_volume((bank->id << 16) | battle_state_of_event_script->operands[0], battle_state_of_event_script->operands[1]);
    return 7;
}

/* 801E8074: Opcode 32 (1 byte): no operation. */
s32 battle_event_script_nop(s32 thread, u8 *insn) {
    return 1;
}

/* 801E807C: Opcode 33 (1 byte): stop the music. */
s32 battle_event_script_stop_music(s32 thread, u8 *insn) {
    if (battle_state_of_event_script->musicPlaying != 0) {
        sound_stop_seq((SoundSeq *)battle_music_seq);
        battle_wait_frame();
        sound_release_seq((SoundSeq *)battle_music_seq);
        battle_state_of_event_script->musicPlaying = 0;
    }
    return 1;
}

/* 801E80E8: Opcode 34 (1 byte): no operation. */
s32 battle_event_script_nop_34(s32 thread, u8 *insn) {
    return 1;
}

/* 801E80F0: Opcode 39 (1 byte): party slot 0 changes to its gear: a formation group of
 * its own (80088490), its sprite sent off and the gear object loaded
 * (800baf48), then out of its group (800883ac); also sets 800ccd88/8006d940
 * = 0, 800cce42 bit 7, 800d32a1 = 2, 800c3eb8 = 1 and two battle state
 * bytes. */
s32 battle_event_script_slot0_to_gear(s32 thread, u8 *insn) {
    battle_work_area.records[0].pilot.gearId = 0;
    game_data.characters[0].gearId = 0;
    battle_give_slot_own_group(0);
    battle_slot_swap_sprite_for_gear(0);
    battle_turn_state->reaction[0] = 1;
    battle_work_area.records[0].flags15A |= 0x80;
    battle_leave_formation_group(0);
    battle_slot_flags[0].unk1 = 2;
    battle_graphics->panels[0].state = 2;
    battle_area.slots[0].gear = 1;
    return 1;
}

/* 801E818C: Opcode 3a (5 bytes; signed operands a, b): start animation b on actor a
 * (801e9430). */
s32 battle_event_script_actor_animation(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 2, 0, 1);
    battle_event_script_start_actor_animation(battle_event_script_get_actor_slot((u8)battle_state_of_event_script->operands[0]), battle_state_of_event_script->operands[1]);
    return 5;
}

/* 801E81EC: Opcode 3b (3 bytes): return actor a to its idle animation (801e950c). */
s32 battle_event_script_actor_idle(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 1, 0, 1);
    battle_event_script_return_actor_to_idle(battle_event_script_get_actor_slot((u8)battle_state_of_event_script->operands[0]));
    return 3;
}

/* 801E823C: Opcode 3c (3 bytes): clear actor a's bytes 0x9e and 0x34 and bits 2-7 of
 * its flags at 0x40 (801e9550). */
s32 battle_event_script_actor_clear(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 1, 0, 1);
    battle_event_script_stop_actor_commands(battle_event_script_get_actor_slot((u8)battle_state_of_event_script->operands[0]));
    return 3;
}

/* 801E828C: Opcode 3d (3 bytes): clear actor a's byte 0x9e (801e958c). */
s32 battle_event_script_actor_clear_9e(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 1, 0, 1);
    battle_event_script_clear_actor_countdown(battle_event_script_get_actor_slot((u8)battle_state_of_event_script->operands[0]));
    return 3;
}

/* 801E82DC: Opcode 3e (9 bytes; signed operands a, x, y, z): move actor a to (x, y, z)
 * (801e95e4) and wait for it. */
s32 battle_event_script_actor_move(s32 thread, u8 *insn) {
    s32 slot;
    s32 length = 0;

    battle_event_script_decode_operands(insn, 4, 0, 1);
    slot = battle_event_script_get_actor_slot((u8)battle_state_of_event_script->operands[0]);
    if (battle_event_script_actions_started[slot] == 0) {
        battle_state_of_event_script->actionRunning[slot] = 1;
        battle_event_script_actions_started[slot] = 1;
        battle_event_script_start_actor_move(slot, battle_state_of_event_script->operands[1], battle_state_of_event_script->operands[2], battle_state_of_event_script->operands[3]);
    } else if (battle_state_of_event_script->actionRunning[slot] == 0) {
        battle_event_script_actions_started[slot] = 0;
        length = 9;
    }
    return length;
}

/* 801E83C0: Opcode 3f (9 bytes; signed operands a, x, y, z): run actor a's action 3
 * with (x, y, z) (801e9694) and wait for it. */
s32 battle_event_script_actor_action(s32 thread, u8 *insn) {
    s32 slot;
    s32 length = 0;

    battle_event_script_decode_operands(insn, 4, 0, 1);
    slot = battle_event_script_get_actor_slot((u8)battle_state_of_event_script->operands[0]);
    if (battle_event_script_actions_started[slot] == 0) {
        battle_state_of_event_script->actionRunning[slot] = 1;
        battle_event_script_actions_started[slot] = 1;
        battle_event_script_start_actor_animation3(slot, battle_state_of_event_script->operands[1], battle_state_of_event_script->operands[2], battle_state_of_event_script->operands[3]);
    } else if (battle_state_of_event_script->actionRunning[slot] == 0) {
        battle_event_script_actions_started[slot] = 0;
        length = 9;
    }
    return length;
}

/* 801E84A4: Opcode 45 (9 bytes; signed operands a-d): actor a attacks actor b
 * (animation c, value d) and waits; value d becomes b's code in the first
 * presentation event. */
s32 battle_event_script_attack(s32 thread, u8 *insn) {
    s32 length = 0;
    u8 attacker;
    u8 target;

    battle_event_script_decode_operands(insn, 4, 0, 1);
    attacker = battle_event_script_get_actor_slot((u8)battle_state_of_event_script->operands[0]);
    target = battle_event_script_get_actor_slot((u8)battle_state_of_event_script->operands[1]);
    battle_turn_state->eventCount = 0;
    battle_clear_event_results();
    battle_area.events[0].codes[target] = battle_state_of_event_script->operands[3];
    if (battle_event_script_actions_started[attacker] == 0) {
        battle_state_of_event_script->actionRunning[attacker] = 1;
        battle_event_script_actions_started[attacker] = 1;
        battle_event_script_turn_actor_to_target(attacker, target);
        while (cd_get_pending_read_count() != 0) {
            battle_wait_frame();
        }
        battle_event_script_start_actor_animation(attacker, battle_state_of_event_script->operands[2]);
    } else if (battle_state_of_event_script->actionRunning[attacker] == 0) {
        battle_event_script_actions_started[attacker] = 0;
        length = 9;
    }
    return length;
}

/* 801E8600: Opcode 46 (7 bytes; signed operands a, b, c): clear the event results,
 * give actor a its idle animation with command c (801e9700) and run its
 * attack on actor b (801e9760). */
s32 battle_event_script_actor_attack(s32 thread, u8 *insn) {
    u8 actor;
    u8 target;

    battle_turn_state->eventCount = 0;
    battle_clear_event_results();
    battle_event_script_decode_operands(insn, 3, 0, 1);
    actor = battle_event_script_get_actor_slot((u8)battle_state_of_event_script->operands[0]);
    target = battle_event_script_get_actor_slot((u8)battle_state_of_event_script->operands[1]);
    battle_event_script_give_actor_command(actor, battle_state_of_event_script->operands[2]);
    battle_event_script_run_actor_attack(actor, target);
    return 7;
}

/* 801E86AC: Opcode 47 (1 byte): stop the disc read and finish the loads (800b8d7c). */
s32 battle_event_script_finish_loads(s32 thread, u8 *insn) {
    battle_stop_reads_finish_loads();
    return 1;
}

/* 801E86D0: Opcode 42 (1 byte): show member 0's number lists (8007ff14(0)). */
s32 battle_event_script_show_gear_hud(s32 thread, u8 *insn) {
    battle_gear_hud_show(0);
    return 1;
}

/* 801E86F4: Opcode 43 (1 byte): leave member 0's menu (800800e8(0)). */
s32 battle_event_script_leave_member_menu(s32 thread, u8 *insn) {
    battle_leave_member_menu(0);
    return 1;
}

/* 801E8718: Opcode 44 (1 byte): clear byte 0x35 of the eleven battle objects. */
s32 battle_event_script_clear_objects_35(s32 thread, u8 *insn) {
    s32 i;

    for (i = 0; i < 11; i++) {
        if (battle_objects[i] != NULL) {
            battle_objects[i]->field35 = 0;
        }
    }
    return 1;
}

/* 801E8750: Opcode 48 (5 bytes; signed operands a, b): play battle sound a (variant b)
 * to its end (800b838c). */
s32 battle_event_script_battle_sound(s32 thread, u8 *insn) {
    battle_event_script_decode_operands(insn, 2, 0, 1);
    battle_play_sound_to_end(battle_state_of_event_script->operands[0], battle_state_of_event_script->operands[1]);
    return 5;
}

/* 801E879C: Run the script: each pass gives every thread in run order a battle frame
 * (800716d8) and then up to four instructions (fewer when one ends it).
 * Passes repeat until opcode 22 makes one the last. Opcodes 4c-ff have no
 * case: the previous length is applied again. */
void battle_event_script_run(void) {
    s32 length;
    u8 steps;
    u32 i;
    u8 thread;
    u8 again;
    u16 pc;

    battle_load_wave_bank_5();
    battle_highlight_slots(0);
    again = 1;
    if (battle_state_of_event_script->halted == 0) {
        do {
            for (i = 0; i < battle_file_of_event_script->threadCount; i++) {
                thread = battle_state_of_event_script->order[i];
                steps = 4;
                battle_wait_frame();
                do {
                    pc = battle_event_script_select_top_level(&battle_state_of_event_script->threads[thread]);
                    switch (battle_state_of_event_script->code[pc]) {
                    case 0x00:
                        length = battle_event_script_end(thread);
                        steps = 1;
                        break;
                    case 0x01:
                        length = battle_event_script_jump(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x02:
                        length = battle_event_script_branch_unless(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x03:
                        length = battle_event_script_request(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x04:
                        length = battle_event_script_request_wait_start(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x05:
                        length = battle_event_script_request_wait_end(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x06:
                        length = battle_event_script_set(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x07:
                        length = battle_event_script_set_one(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x08:
                        length = battle_event_script_set_zero(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x09:
                        length = battle_event_script_add(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x0A:
                        length = battle_event_script_subtract(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x0B:
                        length = battle_event_script_or(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x0C:
                        length = battle_event_script_clear_bits(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x0D:
                        length = battle_event_script_increment(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x0E:
                        length = battle_event_script_decrement(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x0F:
                        length = battle_event_script_and(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x10:
                        length = battle_event_script_or_10(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x11:
                        length = battle_event_script_xor(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x12:
                        length = battle_event_script_shift_left(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x13:
                        length = battle_event_script_shift_right(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x14:
                        length = battle_event_script_random(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x15:
                        length = battle_event_script_random_below(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x16:
                        length = battle_event_script_multiply(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x17:
                        length = battle_event_script_divide(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x18:
                        length = battle_event_script_message(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x19:
                        length = battle_event_script_message_from(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x1A:
                        length = battle_event_script_window(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x1B:
                        length = battle_event_script_speaker(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x1C:
                        length = battle_event_script_enter_script_mode(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x1D:
                        length = battle_event_script_enter_turn_mode(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x1E:
                        length = battle_event_script_fade_white(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x1F:
                        length = battle_event_script_fade_black(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x20:
                        length = battle_event_script_halt_until_battle_end(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x21:
                        length = battle_event_script_skip_result_screens(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x22:
                        length = battle_event_script_last_pass(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x23:
                        length = battle_event_script_act(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x24:
                        length = battle_event_script_next_battle(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x25:
                        length = battle_event_script_allow_defeat(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x26:
                        length = battle_event_script_set_saved_map(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x27:
                        length = battle_event_script_request_movie(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x28:
                        length = battle_event_script_fade(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x29:
                        length = battle_event_script_quake(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x2A:
                        length = battle_event_script_model_animation(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x2B:
                        length = battle_event_script_wait(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x2C:
                        length = battle_event_script_run_order(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x2D:
                        length = battle_event_script_music(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x2E:
                        length = battle_event_script_music_silent(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x2F:
                        length = battle_event_script_music_fade(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x30:
                        length = battle_event_script_music_volume(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x31:
                        length = battle_event_script_sound(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x32:
                        length = battle_event_script_nop(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x33:
                        length = battle_event_script_stop_music(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x34:
                        length = battle_event_script_nop_34(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x35:
                        length = battle_event_script_load_model(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x36:
                        length = battle_event_script_free_model(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x37:
                        length = battle_event_script_exit_battle(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x38:
                        length = battle_event_script_act_nowait(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x39:
                        length = battle_event_script_slot0_to_gear(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x3A:
                        length = battle_event_script_actor_animation(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x3B:
                        length = battle_event_script_actor_idle(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x3C:
                        length = battle_event_script_actor_clear(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x3D:
                        length = battle_event_script_actor_clear_9e(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x3E:
                        length = battle_event_script_actor_move(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x3F:
                        length = battle_event_script_actor_action(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x40:
                        length = battle_event_script_free_model_camera(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x41:
                        length = battle_event_script_sound_volume(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x42:
                        length = battle_event_script_show_gear_hud(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x43:
                        length = battle_event_script_leave_member_menu(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x44:
                        length = battle_event_script_clear_objects_35(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x45:
                        length = battle_event_script_attack(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x46:
                        length = battle_event_script_actor_attack(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x47:
                        length = battle_event_script_finish_loads(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x48:
                        length = battle_event_script_battle_sound(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x49:
                        length = battle_event_script_set_return_fade(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x4A:
                        length = battle_event_script_set_slot0_attack_level4(thread, battle_state_of_event_script->code + pc);
                        break;
                    case 0x4B:
                        length = battle_event_script_suppress_gear_hp_warning(thread, battle_state_of_event_script->code + pc);
                        break;
                    }
                    battle_state_of_event_script->threads[thread].pc[battle_state_of_event_script->threads[thread].level] =
                        length + battle_state_of_event_script->threads[thread].pc[battle_state_of_event_script->threads[thread].level];
                } while (--steps != 0);
            }
            if (battle_state_of_event_script->unk801 != 0 && --battle_state_of_event_script->unk801 == 1) {
                again = 0;
            }
        } while (again);
    }
}
