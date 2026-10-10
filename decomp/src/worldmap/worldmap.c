/* World map unit 80070CFC-80072238 (rodata 8006FAF0-8006FAF4): the overlay
 * entry, the frame loop with its pause, menu, encounter and gear-boarding
 * checks, the open map's frame step, the area file sets and their loaders.
 *
 * The overlay's number opens the image as this unit's rodata, the first
 * word of each mode overlay (docs/matching.md). None of these functions
 * owns rodata, so where the next unit's text starts is not measured; the
 * split starts it at that unit's first rodata owner, func_80072238. */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libcd.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/menu.h"
#include "resident/mode.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "worldmap.h"
#include "camera.h"
#include "effect.h"
#include "party.h"
#include "scene.h"
#include "screen.h"
#include "stream.h"
#include "terrain.h"

/* The frame loop and the area's file set choice, which the overlay entry
 * calls ahead of their definitions. */
void func_800712D0(void);
void func_80071B9C(s32 index, s32 position);

/* The overlay's number, the first word of each mode overlay (field 4, world
 * map 5, battle 6, menu 7, movie 8). */
const s32 D_8006FAF0 = 5;

/* Overlay entry: set up the display, start a new game's world state if none
 * is set, enter the requested mode and run its main loop until the world
 * map is left, then hand over to the next scene. */
void func_80070CFC(void) {
    void (*step)(void);
    void *data;
    RECT rect;
    s32 mode;
    s32 i;

    func_800762FC();
    DrawSync(0);
    VSync(0);
    VSyncCallback(pad_vblank_callback);
    InitGeom();
    sprite_in_worldmap = 1;
    if (game_data_worldmap_flag_word[0] == 0) {
        game_data.map = 0x400;
        game_data.entry[1] = 0xFFF;
        game_data.entry[0] = 0xC00;
        game_data_worldmap_flag_word[0] = 1;
        game_data.worldmap.flags = 0x4003;
        game_data.worldmap.unk6A = 1;
        game_data.worldmap.unk60 = 0x6680;
        game_data.worldmap.unk62 = 0xFF00;
        game_data.worldmap.unk64 = 0x2A00;
        game_data.worldmap.vehicle_heading = 0;
        game_data.worldmap.x = 0x7580;
        game_data.worldmap.z = 0x2C00;
        game_data.worldmap.heading = 0;
        game_data.party[1] = 0xA;
        game_data.party[2] = 5;
        game_data.party[0] = 0;
        game_data.inGear[0] = 0;
        game_data.inGear[1] = 0;
        game_data.inGear[2] = 0;
        game_data.characters[0].gearId = 0xF;
        game_data.characters[1].gearId = 2;
        game_data.characters[2].gearId = 3;
        game_data.characters[3].gearId = 4;
        game_data.characters[4].gearId = 5;
        game_data.characters[5].gearId = 6;
        game_data.characters[6].gearId = 9;
        game_data.characters[7].gearId = 7;
        game_data.characters[8].gearId = 8;
        game_data.characters[9].gearId = 3;
        game_data.characters[10].gearId = 9;
        VEHICLE_SPOTS[0].flags = 0x400;
        VEHICLE_SPOTS[0].x = 0x7500;
        VEHICLE_SPOTS[0].z = 0x2E58;
        VEHICLE_SPOTS[1].flags = 0x400;
        VEHICLE_SPOTS[1].x = 0x7580;
        VEHICLE_SPOTS[1].z = 0x2E58;
        VEHICLE_SPOTS[2].flags = 0x400;
        VEHICLE_SPOTS[2].x = 0x7600;
        VEHICLE_SPOTS[2].z = 0x2E58;
        game_data.unk1844[2] = 1;
        game_data.unk1844[0] = D_8009AF80[game_data.unk1844[1]];
        game_data.unk1844[1] = D_8009AF90[game_data.unk1844[1]];
        MAP_FLAGS = 0x7FFFFFF;
    }
    heap_select_owner_tag(3, 0);
    cd_select_directory(0x24, 0);
    func_80095F78();
    func_8007369C();
    func_80073300();
    if (game_data_worldmap_flag_word[0] & 0x8000) {
        D_8009C894 = 1;
    } else {
        D_8009C894 = 0;
    }
    D_8009BBC4 = 0;
    mode = game_data_worldmap_flag_word[0] & 0x7FFF;
    D_8009BD0C = (game_data.map & 0x3FFF) - 0x400;
    D_8009D3D4 = game_data.entry[1];
    D_8009C584 = game_data.entry[0];
    D_8009C5A8 = mode;
    game_data.entry[2] = mode;
    func_80071B9C(mode, game_data.vars[0]);
    step = D_8009A058[D_8009C5A8].enter;
    if (step != NULL) {
        step();
    }
    while (D_8009D7CC >= 2) {
        D_8009A058[D_8009C5A8].start();
        func_80097800();
        DrawSync(0);
        VSync(0);
        pad_clear_queue();
        D_8009C894 = D_8009D7CC;
        func_800712D0();
        step = D_8009A058[D_8009C5A8].leave;
        step();
    }
    switch (D_8009D7CC) {
    case 0:
        mode_load_overlay_block(1);
        mode_select_next_mode(1);
        if (D_8009BBC4 == 0) {
            if (D_8009D7D8->kind == 3) {
                func_80094364(&D_8009D55C.target, 3, game_data.vars[0]);
            }
            game_data.map = D_8009D7D8->scene;
            game_data.entry[0] = D_8009BD38.vy;
            game_data.entry[2] = D_8009D7D8->entry;
        }
        game_data.vars[2] = D_8009BD0C + 0x400;
        break;
    case 1:
        mode_load_overlay_block(2);
        mode_select_next_mode(2);
        mode_battle_standalone = 0;
        for (i = 0; i < 3; i++) {
            (&game_data.worldmap.unk70)[i] = game_data.inGear[i];
        }
        sound_stop_all_seqs();
        data = D_8009C614;
        memcpy(mode_music_buffer, data, cd_get_aligned_file_size(D_8009BCC8));
        mode_music_cached_seq = mode_music_seq;
        mode_music_seq = (s32)sound_create_seq((SoundSeqHeader *)mode_music_buffer);
        sound_play_seq((SoundSeq *)mode_music_seq, 0x7F, 0);
        break;
    default:
        mode_select_next_mode(0);
        rect.x = 0;
        rect.y = 0;
        rect.w = 0x13F;
        rect.h = 0x1AF;
        ClearImage(&rect, 0, 0, 0x40);
        DrawSync(0);
        break;
    }
    sprite_in_worldmap = 0;
    func_800762FC();
    mode_dispatch(0);
}

/* The world-map main loop: gather input, flip the display buffers, run the
 * frame, and handle pause, encounters and leaving for another scene until
 * D_8009D554 clears. */
void func_800712D0(void) {
    DisplayBuffer *view;
    /* The live gear-byte base also reaches the party IDs 0x57D bytes earlier. */
    u8 *gear_state = game_data.inGear;
    RECT rect;
    s32 i;
    s32 found;

    D_8009BE3C = &D_8009BBC8[1];
    D_8009D7F0 = 1;
    D_8009D554 = 1;
    do {
        D_8009BD1C = 0;
        D_8009BD14 = 0;
        D_8009CD50 = 0;
        D_8009BD18 = 0;
        D_8009BD10 = 0;
        D_8009CD4C = 0;
        while (pad_dequeue_state() != 0) {
            D_8009CD4C |= pad_port0_held;
            D_8009CD50 |= pad_port1_held;
            D_8009BD10 |= pad_port0_pressed;
            D_8009BD14 |= pad_port1_pressed;
            D_8009BD18 |= pad_port0_repeated;
            D_8009BD1C |= pad_port1_repeated;
        }
        while (func_800967E4() == 3) {
            VSync(0);
        }
        CdSync(1, D_8009C588);
        view = D_8009BBC8;
        if (D_8009BE3C == view) {
            view = &D_8009BBC8[1];
        }
        D_8009BE3C = view;
        D_8009D7F0 = D_8009D7F0 == 0;
        ClearOTagR(view->ot, 0x400);
        sprite_queue_start_fill(D_8009D7F0);
        sprite_build_pending_frames();
        func_80097800();
        DrawSync(0);
        VSync(2);
        boot_check_soft_reset();
        PutDispEnv(&D_8009BE3C->disp);
        PutDrawEnv(&D_8009BE3C->draw);
        if (mode_gear_riding_lock == 0 && D_8009BD34 != 0 && D_8009C178 == 0 && D_8009D804 == 0 &&
            D_8009BD24 == -1 && D_8009CE68 == D_8009BD24 && D_8009D554 != 0 && D_8009D80C == 0) {
            D_8009BD34 = 0;
            if (func_80093F18(&D_8009D55C.target) != 4) {
                for (i = 0; i < 3; i++) {
                    (&game_data.worldmap.unk70)[i] = game_data.inGear[i];
                }
                if (gear_state[0] != 0) {
                    gear_state[2] = 0;
                    gear_state[1] = 0;
                    gear_state[0] = 0;
                } else {
                    if (game_data.characters[(gear_state - 0x57D)[0]].gearId != 0xFF) {
                        gear_state[0] = 1;
                    }
                    if (game_data.characters[(gear_state - 0x57D)[1]].gearId != 0xFF) {
                        gear_state[1] = 1;
                    }
                    if (game_data.characters[(gear_state - 0x57D)[2]].gearId != 0xFF) {
                        gear_state[2] = 1;
                    }
                }
                func_80075D4C();
            }
        } else {
            D_8009BD34 = 0;
        }
        if (D_8009C178 == 0) {
            if (D_8009D804 == 0 && D_8009D554 != 0 && D_8009D80C == 0 && (D_8009BD10 & 0x800)) {
                func_8007634C();
            }
            if (D_8009C178 == 0) {
                if (D_8009D804 == 0 && D_8009D554 != 0 && D_8009D80C == 0 && pad_get_controller_kind(0) == 0) {
                    func_80076594();
                }
                if (D_8009C178 == 0 && D_8009D804 == 0 && D_8009BD24 == -1 &&
                    D_8009CE68 == D_8009BD24 && D_8009D554 != 0 && D_8009D80C != 0) {
                    found = func_80075E7C(&D_8009D55C.target, game_data.vars[0]);
                    if (found == 1) {
                        D_8009D554 = 0;
                        D_8009D7CC = found;
                        mode_battle_kind = 0;
                        game_data.worldmap.unk70 = game_data.inGear[0];
                        game_data.worldmap.unk72 = game_data.inGear[1];
                        game_data.worldmap.unk74 = game_data.inGear[2];
                    }
                }
            }
        }
        D_8009D80C = 0;
        if (D_8009BD10 & 0x100) {
            u16 *camera_mode = &game_data.worldmap.unk76;
            *camera_mode ^= 1;
        }
        if (D_8009C178 == 0 && D_8009D804 != 0 && D_8009D554 != 0) {
            if (D_8009BE10 > 0) {
                if (D_8009BE10 < 4) {
                    func_800758C0();
                    menu_state_screen = 0;
                    menu_state_debug_start = 0;
                    menu_state_screen_parameter = 1;
                    func_800762FC();
                    mode_run_menu();
                    func_800762FC();
                    func_80075B58();
                } else if (D_8009BE10 < 8) {
                    D_8009D554 = 0;
                    D_8009D7CC = 0;
                    D_8009D7D8 = &D_8009B6C4[2];
                    game_data.worldmap.flags |= 0x2000;
                }
            }
        } else {
            D_8009D804 = 0;
        }
        sprite_queue_run_uploads();
        func_80074F2C();
        func_80075104();
        SetGeomOffset(0xA0, D_8009BE0C);
        DrawOTag(D_8009BE3C->ot + 0x3FF);
    } while (D_8009D554 != 0);
    ResetGraph(1);
    if (D_8009D7F0 == 0) {
        rect.x = 0;
        rect.y = 0xD8;
        rect.w = 0x140;
        rect.h = 0xD8;
        MoveImage(&rect, 0, 0);
    }
    func_80096694();
    DrawSync(0);
    VSync(0);
    PutDispEnv(&D_8009BBC8[1].disp);
}

/* Mode step that has nothing to do; always reports done. */
s32 func_80071A50(void) {
    return 1;
}

/* One world-map frame: input, actors, camera, terrain, sky and HUD. */
s32 func_80071A58(void) {
    SVECTOR unused; /* unused in the original; reserves 8 bytes */

    if (D_8009D144 == 0) {
        func_80097440(&D_8009BD40);
    } else {
        func_80097244(&D_8009BD40);
    }
    func_80089748();
    func_80089C78();
    func_80085CDC();
    func_8008615C();
    func_800747DC();
    func_800848F4();
    func_800980D4(&D_8009BBB4);
    if (D_8009D558 != 0) {
        func_800981C8(&D_8009BE28);
        func_80096130();
        func_80098CC0();
    }
    func_800983A0(&D_8009BE28);
    func_8009932C(D_8009BE3C->ot, (s32)D_8009BE3C->packets, &D_8009BE28);
    D_8009C5BC += 0x40;
    func_80073B04();
    func_800737EC();
    func_80086798();
    if (game_data.worldmap.unk76 == 0) {
        func_800740B8();
    }
    return 1;
}

/* Select the file set of an area (by index, or for the low indices by the
 * position against the threshold table) and derive its file numbers. */
void func_80071B9C(s32 index, s32 position) {
    WorldmapArea *area;
    s32 i;

    if (index < 8) {
        for (i = 1; position >= D_8009B564[i]; i++) {
        }
        area = &D_8009B584[i - 1];
        D_8009C610 = i - 1;
    } else {
        area = &D_8009B584[index + 1];
    }
    D_8009D3C4 = area->file + 1;
    D_8009C17C = area->file + 2;
    D_8009C174 = area->file + 3;
    D_8009CC98 = area->file + 4;
    D_8009D3D0 = area->file + 5;
    D_8009D3C8 = area->file + 6;
    D_8009D800 = area->file + 7;
    D_8009BCC8 = area->file + 8;
    D_8009BCD8 = area->file + 9;
    D_8009BD08 = area->file + 10;
    D_8009D160 = area->param2;
    D_8009D2B4 = area->param4;
    D_8009D7CC = area->param6;
}

/* Allocate buffers for each party member's model and gear model, then read
 * them all with one list. */
void func_80071CDC(void) {
    s32 i;
    s32 j;
    s32 member;
    u8 gear;

    for (i = 0; i < 3; i++) {
        member = game_data.party[i];
        if (member != 0xFF) {
            D_8009CD34[i] = heap_alloc(cd_get_aligned_file_size(member + 2), 0);
            j = game_data.characters[member].gearId;
            if (j != 0xFF) {
                D_8009BDF8[i] = heap_alloc(cd_get_aligned_file_size(j + 0x13), 0);
            } else {
                D_8009BDF8[i] = NULL;
            }
        } else {
            D_8009BDF8[i] = NULL;
            D_8009CD34[i] = NULL;
        }
    }
    i = 0;
    D_8009C170 = 0;
    for (j = 0; j < 3; j++) {
        member = game_data.party[j];
        if (member != 0xFF) {
            D_8009D3F8[i].file = member + 2;
            D_8009D3F8[i].destination = D_8009CD34[j];
            i++;
            D_8009C170++;
            gear = game_data.characters[member].gearId;
            if (gear != 0xFF) {
                D_8009D3F8[i].file = gear + 0x13;
                D_8009D3F8[i].destination = D_8009BDF8[j];
                i++;
            }
        }
    }
    D_8009D3F8[i].file = 0;
    D_8009D3F8[i].destination = NULL;
    cd_read_file_list(D_8009D3F8, 0, 0);
}

/* Allocate and read the three area files into their resident buffers. */
void func_80071EF0(void) {
    D_8009C59C = heap_alloc(cd_get_aligned_file_size(D_8009C17C), 1);
    D_8009BD20 = heap_alloc(cd_get_aligned_file_size(D_8009C174), 1);
    D_8009C180 = heap_alloc(cd_get_aligned_file_size(D_8009D3C4), 1);
    D_8009D3F8[0].file = D_8009D3C4;
    D_8009D3F8[1].file = D_8009C17C;
    D_8009D3F8[2].file = D_8009C174;
    D_8009D3F8[3].file = 0;
    D_8009D3F8[0].destination = D_8009C180;
    D_8009D3F8[1].destination = D_8009C59C;
    D_8009D3F8[2].destination = D_8009BD20;
    D_8009D3F8[3].destination = NULL;
    cd_read_file_list(D_8009D3F8, 0, 0);
}

/* Allocate and read the two shared world-map files (0x25, 0x26). */
void func_80071FEC(void) {
    menu_state_resource_file = heap_alloc(cd_get_aligned_file_size(0x26), 1);
    D_8009D528 = heap_alloc(cd_get_aligned_file_size(0x25), 1);
    D_8009D3F8[0].file = 0x25;
    D_8009D3F8[0].destination = D_8009D528;
    D_8009D3F8[1].file = 0x26;
    D_8009D3F8[1].destination = menu_state_resource_file;
    D_8009D3F8[2].file = 0;
    D_8009D3F8[2].destination = NULL;
    cd_read_file_list(WORLD_READ_LIST, 0, 0);
}

/* Allocate the area's five file buffers and read the zero-terminated list. */
void func_80072090(void) {
    mode_worldmap_area_load_count++;
    D_8009D3F8[0].file = D_8009CC98;
    D_8009D3F8[0].destination = D_8009C88C = heap_alloc(cd_get_aligned_file_size(D_8009CC98), 1);
    D_8009D3F8[1].file = D_8009D3D0;
    D_8009D3F8[1].destination = D_8009C884 = heap_alloc(cd_get_aligned_file_size(D_8009D3D0), 0);
    D_8009D3F8[2].file = D_8009D3C8;
    D_8009D3F8[2].destination = sound_effect_bank = heap_alloc(cd_get_aligned_file_size(D_8009D3C8), 0);
    D_8009D3F8[3].file = D_8009D800;
    D_8009D3F8[3].destination = D_8009C888 = heap_alloc(cd_get_aligned_file_size(D_8009D800), 0);
    D_8009D3F8[4].file = D_8009BCC8;
    D_8009D3F8[4].destination = D_8009C614 = heap_alloc(cd_get_aligned_file_size(D_8009BCC8), 0);
    D_8009D3F8[5].file = 0;
    D_8009D3F8[5].destination = NULL;
    cd_read_file_list(WORLD_READ_LIST, 0, 0);
}

/* Allocate and read the area's sixth file (kept, mode 0). */
void func_800721E4(void) {
    sound_effect_bank = heap_alloc(cd_get_aligned_file_size(D_8009D3C8), 0);
    cd_read_file(D_8009D3C8, sound_effect_bank, 0, 0);
}
