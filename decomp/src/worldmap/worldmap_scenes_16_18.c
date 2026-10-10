/* World map unit 800811C0-80083A00 (rodata 800702E4-80070490, data
 * 8009A6C0-8009AD2C): the director of scene mode 16 and its actors (the
 * zooming camera, the fading objects and the heat haze), and modes 17 and 18:
 * their set-up and leave handlers, mode 17's camera and pulsing effects, the
 * actor scripts of both and the start of mode 18's camera.
 *
 * worldmap_scene13_director_update's 65-entry table ends at 800702e4 and worldmap_scene16_director_update's
 * follows at once, 4 mod 8, a phase change without a pad word: this unit's
 * rodata starts there and its text after worldmap_scene13_director_update, at or before
 * worldmap_scene16_director_update. Its data opens with mode 16's cue sequence, which
 * worldmap_scene16_director_start, left in the preceding unit by the split, starts. */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/cd.h"
#include "resident/gamedata.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "worldmap.h"
#include "camera.h"
#include "effect.h"
#include "scene.h"
#include "screen.h"
#include "stream.h"
#include "terrain.h"

/* The director's cue sequence, user-supplied script data (an asset in
 * worldmap.classification.txt): 37 u16 states and 37 u16 waits (started by
 * worldmap_scene16_director_start; tools/analysis/overlay_scripts.py decodes it). The waits'
 * 0x4C bytes end with the stray halfword (0x2E07) the original object keeps
 * in its alignment padding. */
INCLUDE_ASSET(".data", worldmap_scene16_cue_states, 0x8009A6C0, 0x4A);
INCLUDE_ASSET(".data", worldmap_scene16_cue_waits, 0x8009A70C, 0x4C);

/* Actor script (worldmap_actor_script_run commands) given by worldmap_scene17_script_start: 434 signed
 * halfwords, user-supplied (an asset in worldmap.classification.txt;
 * tools/analysis/overlay_scripts.py decodes it). */
INCLUDE_ASSET(".data", worldmap_scene17_actor_script, 0x8009A758, 0x364);

/* The pulsing effect's settings for its commands 1, 2 and 4 (worldmap_scene17_pulse_update):
 * five slots of 14, the position and then the actor's parameters. */
s16 worldmap_scene17_pulse_settings_1[5 * 14] = { /* 8009AABC */
    2673, -876, 18618, 64, 128, 255, 2, 1, 0, -64, 0, 0, 20, 0,
    2673, -844, 18682, 96, 96, 255, -4, -2, 0, -32, 1024, 0, 18, 0,
    2673, -748, 18746, 128, 64, 255, 8, 8, 0, 0, 2048, 0, 24, 0,
    2673, -860, 18874, 96, 96, 255, -6, -4, 0, 32, 3072, 0, 22, 0,
    2673, -876, 18938, 64, 128, 255, 2, 1, 0, 64, 1536, 0, 16, 0,
};
s16 worldmap_scene17_pulse_settings_2[5 * 14] = { /* 8009AB48 */
    28280, -1248, 10200, 64, 128, 255, 2, 1, 0, -640, 0, 0, 20, 0,
    28280, -1216, 10264, 96, 96, 255, -4, -2, 0, -608, 1024, 0, 18, 0,
    28280, -1120, 10328, 128, 64, 255, 8, 8, 0, -576, 2048, 0, 24, 0,
    28280, -1232, 10456, 96, 96, 255, -6, -4, 0, -544, 3072, 0, 22, 0,
    28280, -1248, 10520, 64, 128, 255, 2, 1, 0, -512, 1536, 0, 16, 0,
};
s16 worldmap_scene17_pulse_settings_4[5 * 14] = { /* 8009ABD4 */
    20344, -760, 6474, 64, 128, 255, 2, 1, 0, -64, 0, 0, 20, 0,
    20344, -728, 6538, 96, 96, 255, -4, -2, 0, -32, 1024, 0, 18, 0,
    20344, -632, 6602, 128, 64, 255, 8, 8, 0, 0, 2048, 0, 24, 0,
    20344, -744, 6730, 96, 96, 255, -6, -4, 0, 32, 3072, 0, 22, 0,
    20344, -760, 6794, 64, 128, 255, 2, 1, 0, 64, 1536, 0, 16, 0,
};

/* Actor script (worldmap_actor_script_run commands) given by worldmap_scene18_script_start: 102 signed
 * halfwords, user-supplied like worldmap_scene17_actor_script. */
INCLUDE_ASSET(".data", worldmap_scene18_actor_script, 0x8009AC60, 0xCC);

/* 800811C0: Heat-haze scene director (mode 16): worldmap_scene14_director_update's cue sequencer on
 * worldmap_scene16_cue_states/worldmap_scene16_cue_waits. Actor slots (worldmap_scene16_start): 0 the screen fade, 2
 * the camera (worldmap_scene16_camera_update), 3 object 2's fade (worldmap_scene16_fade_object2_update), 4 objects
 * 0 and 1 (worldmap_scene16_fade_objects_0_1_update), 6 the heat-haze strength (worldmap_scene16_haze_strength_update). The
 * fade with rate 2 subtracts the fade quad (black). */
s32 worldmap_scene16_director_update(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    switch (actor->state) {
    /* 0: idle. */
    case 0:
        break;
    /* 1: wait, then fetch the next entry. */
    case 1:
        if (--actor->wait < 0) {
            actor->state = worldmap_scene16_cue_states[actor->u.step];
            actor->wait = worldmap_scene16_cue_waits[actor->u.step];
            actor->u.step++;
        }
        break;
    /* 2: slot 6 request 1. */
    case 0x2:
        worldmap_actor_request(6, 1);
        actor->state = 1;
        break;
    /* 3: slot 6 request 0, which no case of worldmap_scene16_haze_strength_update takes. */
    case 0x3:
        worldmap_actor_request(6, 0);
        actor->state = 1;
        break;
    /* 4: slot 6 request 2. */
    case 0x4:
        worldmap_actor_request(6, 2);
        actor->state = 1;
        break;
    /* 5: slot 6 request 3. */
    case 0x5:
        worldmap_actor_request(6, 3);
        actor->state = 1;
        break;
    /* 6: slot 6 request 4. */
    case 0x6:
        worldmap_actor_request(6, 4);
        actor->state = 1;
        break;
    /* 7: slot 6 request 5. */
    case 0x7:
        worldmap_actor_request(6, 5);
        actor->state = 1;
        break;
    /* 8: area sounds 0x2E-0x30. */
    case 0x8:
        sound_play_effect((sound_effect_bank->id << 16) | 0x2E);
        sound_play_effect((sound_effect_bank->id << 16) | 0x2F);
        sound_play_effect((sound_effect_bank->id << 16) | 0x30);
        actor->state = 1;
        break;
    /* 0x10: slots 3 and 4 request 1. */
    case 0x10:
        worldmap_actor_request(3, 1);
        worldmap_actor_request(4, 1);
        actor->state = 1;
        break;
    /* 0x11: slot 3 request 2. */
    case 0x11:
        worldmap_actor_request(3, 2);
        actor->state = 1;
        break;
    /* 0x3F: fade the music out over 0xF0 frames; fade out at rate 2, 4 per
     * frame. */
    case 0x3F:
        sound_set_seq_fade((SoundSeq *)mode_music_seq, 0, 0xF0);
        worldmap_actor_request(0, 0xD);
        worldmap_screen_fade_rate = 2;
        worldmap_screen_fade_step = 4;
        actor->state = 1;
        break;
    /* 0x40: end the world-map loop with exit 0; idle. */
    case 0x40:
        worldmap_loop_running = 0;
        worldmap_loop_result = 0;
        actor->state = 0;
        break;
    }
    return 1;
}

/* 800813E8: Start a scripted camera at the player position. */
s32 worldmap_scene16_camera_start(s32 index) {
    WorldmapActor *actor;

    worldmap_view_kind = 0;
    actor = &worldmap_actor_slots[index];
    actor->unk7C = 0x1000;
    worldmap_view_center_y = 0x78;
    worldmap_camera_follow_target.target.vx = worldmap_camera.target.vx = worldmap_player_position.vx;
    worldmap_camera_follow_target.target.vy = worldmap_camera.target.vy = worldmap_player_position.vy;
    worldmap_camera_follow_target.target.vz = worldmap_camera.target.vz = worldmap_player_position.vz;
    actor->unk4 = 1;
    actor->state = 0;
    return 1;
}

/* 80081470: Scripted zoom-in camera with a random vertical shake (command 1 starts the zoom). */
s32 worldmap_scene16_camera_update(s32 index) {
    WorldmapActor *actor;
    ActorScratch *scratch;
    s32 delta;

    scratch = (ActorScratch *)0x1F800000;
    actor = &worldmap_actor_slots[index];
    switch (actor->unk4) {
    case 2:
        break;
    case 3:
        break;
    case 1:
        actor->state = 1;
        actor->unk4 = 0;
        worldmap_camera_angle.vx = -0x80;
        worldmap_camera_angle.vy = -0x200;
        worldmap_camera_angle.vz = 0;
        actor->u.step = 0x980000;
        actor->unk54 = actor->unk58 = worldmap_camera_angle.vx << 12;
        worldmap_camera_distance = 0x980000;
        actor->unk5C = actor->unk60 = worldmap_camera_angle.vy << 12;
        break;
    }
    if (worldmap_view_kind == 0) {
        worldmap_camera_place_orbit(&worldmap_view_setup, &worldmap_camera, worldmap_camera_distance, &worldmap_camera_angle);
    }
    if (actor->state == 1) {
        if ((actor->u.step -= 0x10000) < 0x630000) {
            actor->u.step = 0x630000;
        }
        delta = actor->u.step - worldmap_camera_distance;
        if (delta != 0) {
            worldmap_camera_distance += delta >> 5;
        }
        if ((actor->unk54 += 0x1000) > 0x10000) {
            actor->unk54 = 0x10000;
        }
        delta = actor->unk54 - actor->unk58;
        if (delta != 0) {
            actor->unk58 += delta >> 4;
            worldmap_camera_angle.vx = actor->unk58 >> 12;
        }
        if ((actor->unk5C += 0x10000) > 0x4B0000) {
            actor->unk5C = 0x4B0000;
        }
        delta = actor->unk5C - actor->unk60;
        if (delta != 0) {
            actor->unk60 += delta >> 5;
            worldmap_camera_angle.vy = actor->unk60 >> 12;
        }
    }
    scratch->position.vy = rand() % (actor->unk7C >> 12) - (actor->unk7C >> 13);
    VIEW.eye.vy += scratch->position.vy;
    VIEW_VECTORS[1].vy += scratch->position.vy;
    return 1;
}

/* 800816DC: Build `count` semi-transparent textured quads on page 0x180,0. */
void worldmap_scene16_build_translucent_quads(SceneObject *object, POLY_FT4 *quads, s32 count, s32 abr) {
    s32 i;

    for (i = 0; i < count; i++) {
        setPolyFT4(quads);
        quads->tpage = GetTPage(0, abr, 0x180, 0);
        setSemiTrans(quads, 1);
        quads++;
    }
    memcpy(object->prims2, object->prims, count * sizeof(POLY_FT4));
}

/* 800817A0: Start the actor above the player and build scene object 2 there. */
s32 worldmap_scene16_fade_object2_start(s32 index) {
    WorldmapActor *actor;
    SceneObject *objects;

    actor = &worldmap_actor_slots[index];
    objects = worldmap_objects;
    actor->state = 0;
    actor->position.vx = worldmap_player_position.vx;
    actor->position.vy = worldmap_player_position.vy - 0x100000;
    actor->position.vz = worldmap_player_position.vz;
    actor->u.step = 0;
    actor->unk54 = 0;
    actor->unk58 = 0;
    worldmap_scene16_build_translucent_quads(&objects[2], objects[2].prims, objects[2].def->primitive_count, 1);
    objects[2].position.vx = actor->position.vx >> 12;
    objects[2].position.vy = actor->position.vy >> 12;
    objects[2].position.vz = actor->position.vz >> 12;
    return 1;
}

/* 80081868: Fade scene object 2 in (command 1) or reset it to opaque grey (command 2). */
s32 worldmap_scene16_fade_object2_update(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    POLY_FT4 *quad;
    s32 i;

    actor = &worldmap_actor_slots[index];
    object = &worldmap_objects[2];
    switch (actor->unk4) {
    case 1:
        actor->unk4 = 0;
        actor->state = 1;
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 0;
        quad = (&object->prims)[worldmap_display_buffer_index];
        for (i = 0; i < object->def->primitive_count; i++) {
            setSemiTrans(quad, 0);
            setRGB0(quad, 0x80, 0x80, 0x80);
            quad++;
        }
        break;
    }
    switch (actor->state) {
    case 0:
        break;
    case 1:
        actor->u.step += 1;
        actor->unk54 += 1;
        actor->unk58 += 1;
        if (actor->u.step >= 0xFF) {
            actor->unk58 = 0xFF;
            actor->unk54 = 0xFF;
            actor->u.step = 0xFF;
            actor->state = 0;
        }
        break;
    }
    worldmap_set_quad_colors((&object->prims)[worldmap_display_buffer_index], object->def->primitive_count, actor->u.step, actor->unk54, actor->unk58);
    return 1;
}

/* 800819C8: Place the actor at the player and rebuild scene objects 0-1 stretched 7x in height. */
s32 worldmap_scene16_fade_objects_0_1_start(s32 index) {
    SceneObject *objects;
    WorldmapActor *actor;

    objects = worldmap_objects;
    actor = &worldmap_actor_slots[index];
    actor->state = 0;
    actor->position.vx = worldmap_player_position.vx;
    actor->position.vy = worldmap_player_position.vy;
    actor->position.vz = worldmap_player_position.vz;
    actor->u.step = 0;
    actor->unk54 = 0;
    actor->unk58 = 0;
    objects[0].matrix = worldmap_identity_matrix;
    SCALE_SCRATCH->scale[0].vx = SCALE_SCRATCH->scale[0].vz = 0x1000;
    SCALE_SCRATCH->scale[0].vy = 0x7000;
    ScaleMatrix(&objects[0].matrix, &SCALE_SCRATCH->scale[0]);
    objects[1].matrix = objects[0].matrix;
    worldmap_scene16_build_translucent_quads(objects, objects->prims, objects->def->primitive_count, 3);
    objects++;
    worldmap_scene16_build_translucent_quads(objects, objects->prims, objects->def->primitive_count, 3);
    return 1;
}

/* 80081B24: Fade scene objects 0 and 1 in (command 1 starts it). */
s32 worldmap_scene16_fade_objects_0_1_update(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;

    actor = &worldmap_actor_slots[index];
    object = worldmap_objects;
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        actor->state = 1;
    }
    switch (actor->state) {
    case 0:
        break;
    case 1:
        actor->u.step += 4;
        actor->unk54 += 2;
        actor->unk58 += 1;
        if (actor->u.step >= 0xFC) {
            actor->u.step = 0xFC;
            actor->state = 0;
        }
        break;
    }
    worldmap_set_quad_colors((&object->prims)[worldmap_display_buffer_index], object->def->primitive_count, actor->u.step, actor->unk54, actor->unk58);
    object++;
    worldmap_set_quad_colors((&object->prims)[worldmap_display_buffer_index], object->def->primitive_count, actor->u.step, actor->unk54, actor->unk58);
    return 1;
}

/* 80081C3C: Allocate the shared quad pool (two display copies) and mark every quad free. */
s32 worldmap_scene16_haze_start(void) {
    POLY_FT4 *quads;
    s32 i;
    s16 *flags;

    worldmap_scene16_haze_quads[0] = heap_alloc(sizeof(QuadBuffer), 0);
    worldmap_scene16_haze_quads[1] = heap_alloc(sizeof(QuadBuffer), 0);
    worldmap_scene16_haze_spreads = heap_alloc(0xC0 * sizeof(s16), 0);
    quads = worldmap_scene16_haze_quads[0]->quads;
    for (i = 0; i < 0xC0; i++) {
        setPolyFT4(quads);
        setRGB0(quads, 0x80, 0x80, 0x80);
        setShadeTex(quads, 1);
        quads->tpage = GetTPage(2, 0, 0x280, 0x100);
        quads++;
    }
    *worldmap_scene16_haze_quads[1] = *worldmap_scene16_haze_quads[0];
    flags = worldmap_scene16_haze_spreads;
    for (i = 0; i < 0xC0; i++) {
        *flags++ = 1;
    }
    return 1;
}

/* 80081D80: Heat haze: offset each of 192 one-pixel rows by a random amount and copy the result back to the frame. */
s32 worldmap_scene16_haze_update(void) {
    RECT rect;
    POLY_FT4 *quad;
    u16 *spread;
    s32 row;
    s32 next;
    s32 offset;

    row = 0;
    quad = worldmap_scene16_haze_quads[worldmap_display_buffer_index]->quads;
    spread = worldmap_scene16_haze_spreads;
    do {
        offset = rand() % *spread - (*spread >> 1);
        spread++;
        quad->y0 = row;
        quad->y1 = row;
        quad->v0 = row;
        quad->v1 = row;
        quad->u0 = 0;
        quad->u1 = 0xC0;
        quad->u2 = 0;
        quad->u3 = 0xC0;
        quad->x0 = offset + 0x40;
        quad->x1 = offset + 0x100;
        quad->x2 = offset + 0x40;
        quad->x3 = offset + 0x100;
        next = row + 1;
        quad->y2 = next;
        quad->y3 = next;
        quad->v2 = next;
        quad->v3 = next;
        row = next;
        addPrim(worldmap_current_display_buffer->ot, quad);
        quad++;
    } while (row < 0xC0);
    rect.x = 0x40;
    rect.w = 0xC0;
    rect.h = 0xD8;
    rect.y = worldmap_display_buffer_index * 0xD8;
    SetDrawMove(&worldmap_scene16_haze_copies[worldmap_display_buffer_index], &rect, 0x280, 0x100);
    addPrim(worldmap_current_display_buffer->ot, &worldmap_scene16_haze_copies[worldmap_display_buffer_index]);
    return 1;
}

/* 80081FB4: Reset an actor to state 0, step 1. */
s32 worldmap_scene16_haze_strength_start(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->state = 0;
    actor->u.step = 1;
    return 1;
}

/* 80081FD8: Heat-haze strength: set every row (1-3, 5) or scatter random rows (1, 4) per command. */
s32 worldmap_scene16_haze_strength_update(s32 index) {
    WorldmapActor *actor;
    s32 i;
    s32 start;
    u16 *flags;

    actor = &worldmap_actor_slots[index];
    switch (actor->unk4) {
    case 1:
        actor->unk4 = 0;
        actor->state = 1;
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 2;
        break;
    case 3:
        actor->unk4 = 0;
        actor->state = 3;
        break;
    case 4:
        actor->unk4 = 0;
        actor->state = 4;
        break;
    case 5:
        actor->unk4 = 0;
        actor->state = 5;
        break;
    }
    switch (actor->state) {
    case 0:
        break;
    case 1:
        for (i = 0, flags = worldmap_scene16_haze_spreads; i < 0xC0; i++) {
            *flags++ = actor->u.value;
        }
        start = rand() & 0x3F;
        flags = &worldmap_scene16_haze_spreads[start];
        for (i = start; i < start + (rand() & 0x1F) + 1; i++, flags++) {
            if (!(rand() & 3)) {
                *flags = (rand() & 0x3F) + 1;
            }
        }
        start = (rand() & 0x3F) + 0x40;
        flags = &worldmap_scene16_haze_spreads[start];
        for (i = start; i < start + (rand() & 0x1F) + 1; i++, flags++) {
            if (!(rand() & 3)) {
                *flags = (rand() & 0x3F) + 1;
            }
        }
        start = (rand() & 0x1F) + 0x80;
        flags = &worldmap_scene16_haze_spreads[start];
        for (i = start; i < start + (rand() & 0x1F) + 1; i++, flags++) {
            if (!(rand() & 3)) {
                *flags = (rand() & 0x3F) + 1;
            }
        }
        break;
    case 2:
        flags = worldmap_scene16_haze_spreads;
        if (++actor->u.step > 0x40) {
            actor->u.step = 0x40;
        }
        for (i = 0; i < 0xC0; i++) {
            *flags++ = actor->u.value;
        }
        break;
    case 3:
        flags = worldmap_scene16_haze_spreads;
        if (--actor->u.step < 2) {
            actor->u.step = 2;
        }
        for (i = 0; i < 0xC0; i++) {
            *flags++ = actor->u.value;
        }
        break;
    case 4:
        flags = worldmap_scene16_haze_spreads;
        for (i = 0; i < 0xC0; i++, flags++) {
            if (!(rand() & 3)) {
                *flags = (rand() & 0x3F) + 1;
            }
        }
        break;
    case 5:
        for (i = 0, flags = worldmap_scene16_haze_spreads; i < 0xC0; i++) {
            *flags++ = 2;
        }
        break;
    }
    return 1;
}

/* 80082324: Set up the pulsing-effect scene: fixed start position, music, its camera and five effect slots. */
void worldmap_scene17_start(void) {
    RECT rect;
    SoundSeq *sequence;
    void *data;
    u16 debug;

    worldmap_init_display();
    rect.w = 0x140;
    rect.x = 0;
    rect.y = 0;
    rect.h = 0xD8;
    MoveImage(&rect, 0x2C0, 0x100);
    DrawSync(0);
    worldmap_fade_saved_screen(0x40, 0, 4, 2);
    while (cd_get_pending_read_count() >= 3) {
    }
    worldmap_unpack_scene_area_data();
    worldmap_actor_alloc_slots();
    worldmap_unread_mode_matrix = worldmap_identity_matrix;
    worldmap_screen_fade_rate = 2;
    worldmap_screen_fade_step = 0x10;
    worldmap_menu_requested = 0;
    worldmap_view_kind = 0;
    worldmap_cloud_draw_hook = worldmap_clouds_move;
    worldmap_terrain_compute_cull_normals();
    cd_sync_reads(0);
    mode_stop_music();
    worldmap_player_position.vx = 0x70A9000;
    worldmap_player_position.vy = -0x148000;
    worldmap_player_position.vz = 0x42AA000;
    worldmap_objects_build();
    worldmap_upload_area_image();
    worldmap_terrain_upload_image();
    worldmap_read_area_sound_files();
    worldmap_sky_init();
    worldmap_billboards_resolve_lists();
    worldmap_clouds_scatter();
    worldmap_texture_anim_create();
    worldmap_texture_anim2_create();
    worldmap_horizon_init();
    worldmap_effects_alloc_slots();
    cd_sync_reads(0);
    mode_music_wave_bank = sound_load_wave_bank(worldmap_wave_bank, 0);
    cd_select_directory(0x24, 0);
    worldmap_terrain_reset_at(&worldmap_player_position);
    do {
        worldmap_stream_step();
        VSync(0);
    } while (worldmap_stream_count_queued() > 0);
    debug = sound_driver_flags & 0x10;
    if (debug) {
        while (debug) {
        }
    }
    heap_free(worldmap_wave_bank);
    sound_add_effect_bank(sound_effect_bank);
    data = worldmap_music;
    memcpy(mode_music_buffer, data, cd_get_aligned_file_size(worldmap_music_file));
    sequence = sound_create_seq((SoundSeqHeader *)mode_music_buffer);
    mode_music_seq = (s32)sequence;
    sound_play_seq(sequence, 0x7F, 0);
    worldmap_actor_spawn((s32)worldmap_screen_fade_start, (s32)worldmap_screen_fade_update);
    worldmap_actor_spawn((s32)worldmap_scene17_script_start, (s32)worldmap_actor_script_run);
    worldmap_actor_spawn((s32)worldmap_scene17_camera_start, (s32)worldmap_scene17_camera_update);
    worldmap_actor_spawn((s32)worldmap_scene17_pulse_start, (s32)worldmap_scene17_pulse_update);
    worldmap_actor_spawn((s32)worldmap_scene17_pulse_start, (s32)worldmap_scene17_pulse_update);
    worldmap_actor_spawn((s32)worldmap_scene17_pulse_start, (s32)worldmap_scene17_pulse_update);
    worldmap_actor_spawn((s32)worldmap_scene17_pulse_start, (s32)worldmap_scene17_pulse_update);
    worldmap_actor_spawn((s32)worldmap_scene17_pulse_start, (s32)worldmap_scene17_pulse_update);
    worldmap_actor_spawn((s32)worldmap_scene17_terrain_reload_start, (s32)worldmap_scene17_terrain_reload_update);
    worldmap_actor_spawn((s32)worldmap_scene_frame_billboards_start, (s32)worldmap_scene_frame_billboards_update);
    worldmap_terrain_alloc_packets();
    worldmap_effects_alloc_quads();
    worldmap_clouds_alloc_quads();
    worldmap_billboards_alloc_quads();
    worldmap_encounter_reset_timers();
}

/* 800826B4: Leave the world map for scene 0x269 (flag word 2). */
void worldmap_scene17_leave(void) {
    sound_stop_all_effects();
    sound_remove_effect_bank(sound_effect_bank);
    heap_free(sound_effect_bank);
    worldmap_objects_free();
    worldmap_billboards_free_quads();
    worldmap_clouds_free();
    worldmap_clouds_free_quads();
    worldmap_texture_anim_free();
    worldmap_texture_anim2_free();
    worldmap_effects_free_slots();
    worldmap_effects_free_quads();
    worldmap_terrain_free_blocks();
    heap_free(worldmap_display_buffers[0].ot);
    heap_free(worldmap_display_buffers[1].ot);
    heap_free(worldmap_display_buffers[0].packets);
    heap_free(worldmap_display_buffers[1].packets);
    heap_free(worldmap_area_data);
    worldmap_actor_free_slots();
    game_data.map = 0x269;
    game_data.entry[2] = 2;
    worldmap_next_scene_chosen = 1;
    game_data.entry[0] = worldmap_camera_angle.vy;
}

/* 800827C8: Give an actor its script. */
s32 worldmap_scene17_script_start(s32 index) {
    worldmap_actor_slots[index].u.script = worldmap_scene17_actor_script;
    return 1;
}

/* 800827EC: Start a scripted camera on the player: pitch -0x220 at distance 0x50. */
s32 worldmap_scene17_camera_start(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->unk7C = 0x1000;
    actor->position.vx = worldmap_camera_follow_target.target.vx = worldmap_camera.target.vx = worldmap_player_position.vx;
    actor->position.vy = worldmap_camera_follow_target.target.vy = worldmap_camera.target.vy = worldmap_player_position.vy;
    actor->position.vz = worldmap_camera_follow_target.target.vz = worldmap_camera.target.vz = worldmap_player_position.vz;
    actor->state = 0;
    actor->unk4 = 0;
    actor->unk5C = 0x500000;
    worldmap_camera_angle.vx = -0x220;
    worldmap_camera_angle.vy = 0;
    worldmap_camera_angle.vz = 0;
    actor->motion.vx = actor->u.step = -0x220 << 12;
    worldmap_view_center_y = 0x78;
    worldmap_view_kind = 0;
    worldmap_camera_distance = 0x500000;
    actor->motion.vy = actor->unk54 = worldmap_camera_angle.vy << 12;
    actor->motion.vz = actor->unk58 = worldmap_camera_angle.vz << 12;
    return 1;
}

/* 800828DC: Pulsing-effect scene camera: commands pick camera shots and moves; each state eases distance, pitch and yaw; adds a vertical shake. */
s32 worldmap_scene17_camera_update(s32 index) {
    WorldmapActor *actor;
    ActorScratch *scratch;

    actor = &worldmap_actor_slots[index];
    scratch = (ActorScratch *)0x1F800000;
    switch (actor->unk4) {
    case 1:
        actor->unk4 = 0;
        actor->state = 1;
        actor->u.step = actor->motion.vx;
        actor->unk54 = actor->motion.vy;
        actor->unk58 = actor->motion.vz;
        break;
    case 2:
        actor->unk4 = 0;
        actor->state = 0;
        actor->unk5C = 0x3E0000;
        worldmap_camera_angle.vx = -0x20;
        worldmap_camera_angle.vy = 0xD70;
        worldmap_camera_angle.vz = 0;
        actor->motion.vx = actor->u.step = -0x20 << 12;
        actor->motion.vy = actor->unk54 = worldmap_camera_angle.vy << 12;
        worldmap_camera_distance = 0x3E0000;
        actor->motion.vz = actor->unk58 = worldmap_camera_angle.vz << 12;
        break;
    case 3:
        actor->unk4 = 0;
        actor->state = 2;
        break;
    case 4:
        actor->unk4 = 0;
        actor->state = 3;
        worldmap_camera_follow_target.target.vx = 0x7499000;
        worldmap_camera_follow_target.target.vy = -0x18C000;
        worldmap_camera_follow_target.target.vz = 0x408A000;
        break;
    case 5:
        actor->unk4 = 0;
        actor->state = 4;
        break;
    case 6:
        actor->state = 5;
        actor->unk7C = 0x1000;
        actor->unk4 = 0;
        actor->unk5C = 0x320000;
        worldmap_camera_angle.vx = 0x40;
        worldmap_camera_angle.vy = 0xD40;
        worldmap_camera_angle.vz = 0;
        actor->motion.vx = actor->u.step = 0x40 << 12;
        actor->motion.vy = actor->unk54 = worldmap_camera_angle.vy << 12;
        worldmap_camera_distance = 0x320000;
        actor->motion.vz = actor->unk58 = worldmap_camera_angle.vz << 12;
        actor->position.vx = worldmap_camera_follow_target.target.vx = 0x1379000;
        actor->position.vy = worldmap_camera_follow_target.target.vy = -0x120000;
        actor->position.vz = worldmap_camera_follow_target.target.vz = 0x4B2A000;
        break;
    case 7:
        actor->unk4 = 0;
        actor->state = 6;
        break;
    case 8:
        actor->state = 7;
        actor->unk4 = 0;
        actor->unk5C = 0x100000;
        worldmap_camera_angle.vx = -0x90;
        worldmap_camera_angle.vy = -0x510;
        worldmap_camera_angle.vz = 0;
        actor->motion.vx = actor->u.step = -0x90 << 12;
        actor->motion.vy = actor->unk54 = worldmap_camera_angle.vy << 12;
        worldmap_camera_distance = 0x100000;
        actor->motion.vz = actor->unk58 = worldmap_camera_angle.vz << 12;
        actor->position.vx = worldmap_camera_follow_target.target.vx = 0x7529000;
        actor->position.vy = worldmap_camera_follow_target.target.vy = -0x18C000;
        actor->position.vz = worldmap_camera_follow_target.target.vz = 0x2A3A000;
        break;
    case 9:
        actor->unk4 = 0;
        actor->state = 8;
        break;
    case 10:
        actor->state = 9;
        actor->unk4 = 0;
        actor->unk5C = 0x320000;
        worldmap_camera_angle.vx = -0x160;
        worldmap_camera_angle.vy = 0xDE0;
        worldmap_camera_angle.vz = 0;
        actor->motion.vx = actor->u.step = -0x160 << 12;
        actor->motion.vy = actor->unk54 = worldmap_camera_angle.vy << 12;
        worldmap_camera_distance = 0x320000;
        actor->motion.vz = actor->unk58 = worldmap_camera_angle.vz << 12;
        actor->position.vx = worldmap_camera_follow_target.target.vx = 0x2659000;
        actor->position.vy = worldmap_camera_follow_target.target.vy = -0x110000;
        actor->position.vz = worldmap_camera_follow_target.target.vz = 0x6D8A000;
        break;
    case 11:
        actor->state = 10;
        actor->unk4 = 0;
        actor->unk5C = 0x290000;
        worldmap_camera_angle.vx = -0x1A0;
        worldmap_camera_angle.vy = 0x160;
        worldmap_camera_angle.vz = 0;
        actor->motion.vx = actor->u.step = -0x1A0 << 12;
        actor->motion.vy = actor->unk54 = worldmap_camera_angle.vy << 12;
        worldmap_camera_distance = 0x290000;
        actor->motion.vz = actor->unk58 = worldmap_camera_angle.vz << 12;
        actor->position.vx = worldmap_camera_follow_target.target.vx = 0x4BA9000;
        actor->position.vy = worldmap_camera_follow_target.target.vy = -0x288000;
        actor->position.vz = worldmap_camera_follow_target.target.vz = 0x1DDA000;
        break;
    case 12:
        actor->unk4 = 0;
        actor->state = 11;
        break;
    }
    if (worldmap_view_kind == 0) {
        worldmap_camera_place_orbit(&worldmap_view_setup, &worldmap_camera, worldmap_camera_distance, &worldmap_camera_angle);
    }
    switch (actor->state) {
    case 0:
        break;
    case 1:
        actor->u.step = worldmap_step_value_toward(actor->u.step, -0x20000, 0x5D17);
        actor->unk54 = worldmap_step_value_toward(actor->unk54, 0x580000, 0x10000);
        break;
    case 2:
        actor->u.step = worldmap_step_value_toward(actor->u.step, -0x1F0000, -0x10000);
        actor->unk54 = worldmap_step_value_toward(actor->unk54, 0x1290000, 0x10000);
        actor->unk5C = worldmap_step_value_toward(actor->unk5C, 0x490000, 0x10000);
        break;
    case 3:
        actor->u.step = worldmap_step_value_toward(actor->u.step, -0x10000, 0x10000);
        actor->unk54 = worldmap_step_value_toward(actor->unk54, 0x1290000, 0x10000);
        actor->unk5C = worldmap_step_value_toward(actor->unk5C, 0x2B0000, -0x10000);
        break;
    case 4:
        actor->unk7C += 0x200;
        if (actor->unk7C > 0x8000) {
            actor->unk7C = 0x8000;
            actor->state = 0;
        }
        break;
    case 6:
        actor->unk54 = worldmap_step_value_toward(actor->unk54, 0xF40000, 0x4000);
        break;
    case 8:
        actor->u.step = worldmap_step_value_toward(actor->u.step, 0x50000, 0x2000);
        actor->unk54 = worldmap_step_value_toward(actor->unk54, 0x10000, 0x8000);
        break;
    case 11:
        actor->u.step = worldmap_step_value_toward(actor->u.step, -0x80000, 0x2400);
        actor->unk54 = worldmap_step_value_toward(actor->unk54, 0x2C0000, 0x2C00);
        actor->unk5C = worldmap_step_value_toward(actor->unk5C, 0x100000, -0x3200);
        worldmap_camera_follow_target.target.vx = worldmap_step_value_toward(worldmap_camera_follow_target.target.vx, 0x4CD9000, 0x2600);
        worldmap_camera_follow_target.target.vy = worldmap_step_value_toward(worldmap_camera_follow_target.target.vy, -0x15C000, 0x2580);
        worldmap_camera_follow_target.target.vz = worldmap_step_value_toward(worldmap_camera_follow_target.target.vz, 0x1C8A000, -0x2A00);
        worldmap_wrap_position(&worldmap_camera_follow_target.target);
        break;
    }
    worldmap_camera_ease_angle(actor, scratch);
    worldmap_camera_ease_distance(actor, scratch);
    worldmap_camera_ease_position(actor, scratch);
    scratch->position.vy = rand() % (actor->unk7C >> 12) - (actor->unk7C >> 13);
    VIEW.eye.vy += scratch->position.vy;
    VIEW_VECTORS[1].vy += scratch->position.vy;
    return 1;
}

/* 80082F64: Pulse a scene object: spin it, stretch its x scale and bounce its tint between limits. */
void worldmap_scene17_pulse_object(WorldmapActor *actor, SceneObject *object, ScaleScratch *scratch) {
    switch (actor->state) {
    case 0:
        break;
    case 1:
        scratch->angle.vz = 0;
        scratch->angle.vx = 0;
        scratch->angle.vy = actor->unk68;
        scratch->scale[0].vx = actor->unk70 & 0x7FFF;
        scratch->scale[0].vy = 0x1000;
        scratch->scale[0].vz = gpu_get_sin(actor->unk6C) * 6;
        gpu_build_rotation_matrix(&scratch->angle, &scratch->matrix[0]);
        ScaleMatrix(&scratch->matrix[0], &scratch->scale[0]);
        object->matrix = scratch->matrix[0];
        actor->u.step += actor->unk5C;
        actor->unk54 += actor->unk60;
        if (actor->u.step >= 0x100) {
            actor->u.step = 0xFF;
            actor->unk5C = -actor->unk5C;
        } else if (actor->u.step < 0x40) {
            actor->u.step = 0x40;
            actor->unk5C = -actor->unk5C;
        }
        if (actor->unk54 >= 0x100) {
            actor->unk54 = 0xFF;
            actor->unk60 = -actor->unk60;
        } else if (actor->unk54 < 0x80) {
            actor->unk54 = 0x80;
            actor->unk60 = -actor->unk60;
        }
        actor->unk6C = (actor->unk6C + 8) & 0xFFF;
        actor->unk70 += actor->unk74;
        break;
    }
}

/* 80083108: Build `count` semi-transparent black textured triangles on page 0x2C0,0x100. */
void worldmap_scene17_build_black_triangles(SceneObject *object, POLY_FT3 *prims, s32 count, s32 abr) {
    s32 i;

    for (i = count - 1; i != -1; i--) {
        ((u8 *)prims)[3] = 7;
        prims->code = 0x24;
        prims->tpage = GetTPage(0, abr, 0x2C0, 0x100);
        prims->r0 = 0;
        prims->g0 = 0;
        prims->b0 = 0;
        prims->code |= 2;
        prims++;
    }
    memcpy(object->prims2, object->prims, count * sizeof(POLY_FT3));
}

/* 800831D8: Set the colour of `count` textured triangles. */
void worldmap_set_triangle_colors(POLY_FT3 *prims, s32 count, s32 r, s32 g, s32 b) {
    for (count--; count != -1; count--) {
        setRGB0(prims, r, g, b);
        prims++;
    }
}

/* 80083214: Rebuild the triangles of scene effect `index`. */
s32 worldmap_scene17_pulse_start(s32 index) {
    SceneObject *object;

    object = &worldmap_objects[78 + index];
    worldmap_scene17_build_black_triangles(object, object->prims, object->def->primitive_count, 3);
    return 1;
}

/* 80083264: Pulsing effect slot: commands 1/2/4 load one of three settings, 3 stops it; then pulse and tint its object. */
s32 worldmap_scene17_pulse_update(s32 index) {
    WorldmapActor *actor;
    SceneObject *object;
    s32 *params;
    s32 slot;
    s32 i;

    slot = index - 3;
    actor = &worldmap_actor_slots[index];
    object = &worldmap_objects[81 + slot];
    switch (actor->unk4) {
    case 0:
        break;
    case 1:
        params = &actor->u.step;
        actor->state = 1;
        actor->unk4 = 0;
        object->position.vx = worldmap_scene17_pulse_settings_1[slot * 14];
        object->position.vy = worldmap_scene17_pulse_settings_1[slot * 14 + 1];
        object->position.vz = worldmap_scene17_pulse_settings_1[slot * 14 + 2];
        for (i = 3; i < 14; i++) {
            *params++ = worldmap_scene17_pulse_settings_1[slot * 14 + i];
        }
        break;
    case 2:
        params = &actor->u.step;
        actor->state = 1;
        actor->unk4 = 0;
        object->position.vx = worldmap_scene17_pulse_settings_2[slot * 14];
        object->position.vy = worldmap_scene17_pulse_settings_2[slot * 14 + 1];
        object->position.vz = worldmap_scene17_pulse_settings_2[slot * 14 + 2];
        for (i = 3; i < 14; i++) {
            *params++ = worldmap_scene17_pulse_settings_2[slot * 14 + i];
        }
        break;
    case 3:
        actor->unk4 = 0;
        actor->state = 0;
        break;
    case 4:
        params = &actor->u.step;
        actor->state = 1;
        actor->unk4 = 0;
        object->position.vx = worldmap_scene17_pulse_settings_4[slot * 14];
        object->position.vy = worldmap_scene17_pulse_settings_4[slot * 14 + 1];
        object->position.vz = worldmap_scene17_pulse_settings_4[slot * 14 + 2];
        for (i = 3; i < 14; i++) {
            *params++ = worldmap_scene17_pulse_settings_4[slot * 14 + i];
        }
        break;
    }
    worldmap_scene17_pulse_object(actor, object, (ScaleScratch *)0x1F800000);
    worldmap_set_triangle_colors((&object->prims)[worldmap_display_buffer_index], object->def->primitive_count, actor->u.step, actor->unk54, actor->unk58);
    return 1;
}

/* 800834D0: Mode step that has nothing to do; always reports done. */
s32 worldmap_scene17_terrain_reload_start(void) {
    return 1;
}

/* 800834D8: On command, reload the terrain around the player and drain the frames. */
s32 worldmap_scene17_terrain_reload_update(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    if (actor->unk4 == 1) {
        actor->unk4 = 0;
        DrawSync(0);
        VSync(0);
        worldmap_terrain_free_blocks();
        worldmap_terrain_reset_at(&worldmap_player_position);
        do {
            worldmap_stream_step();
            VSync(0);
        } while (worldmap_stream_count_queued() > 0);
    }
    return 1;
}

/* 8008355C: Set up the effect scene: fixed start position, its director and effect actors. */
void worldmap_scene18_start(void) {
    RECT rect;

    worldmap_init_display();
    rect.w = 0x140;
    rect.x = 0;
    rect.y = 0;
    rect.h = 0xD8;
    MoveImage(&rect, 0x2C0, 0x100);
    DrawSync(0);
    worldmap_fade_saved_screen(0x40, 0, 4, 2);
    while (cd_get_pending_read_count() >= 3) {
    }
    worldmap_unpack_scene_area_data();
    worldmap_actor_alloc_slots();
    worldmap_unread_mode_matrix = worldmap_identity_matrix;
    worldmap_screen_fade_rate = 2;
    worldmap_screen_fade_step = 0x10;
    worldmap_menu_requested = 0;
    worldmap_view_kind = 0;
    worldmap_cloud_draw_hook = worldmap_clouds_move;
    worldmap_terrain_compute_cull_normals();
    cd_sync_reads(0);
    worldmap_read_area_sound_bank();
    worldmap_player_position.vx = 0x1800000;
    worldmap_player_position.vy = -0x100000;
    worldmap_player_position.vz = 0x1A00000;
    worldmap_objects_build();
    worldmap_upload_area_image();
    worldmap_terrain_upload_image();
    worldmap_sky_init();
    worldmap_clouds_scatter();
    worldmap_texture_anim_create();
    worldmap_texture_anim2_create();
    worldmap_horizon_init();
    worldmap_effects_alloc_slots();
    cd_sync_reads(0);
    sound_add_effect_bank(sound_effect_bank);
    cd_select_directory(0x24, 0);
    worldmap_terrain_reset_at(&worldmap_player_position);
    do {
        worldmap_stream_step();
        VSync(0);
    } while (worldmap_stream_count_queued() > 0);
    worldmap_actor_spawn((s32)worldmap_screen_fade_start, (s32)worldmap_screen_fade_update);
    worldmap_actor_spawn((s32)worldmap_scene18_script_start, (s32)worldmap_actor_script_run);
    worldmap_actor_spawn((s32)worldmap_scene18_camera_start, (s32)worldmap_scene18_camera_update);
    worldmap_actor_spawn((s32)worldmap_scene18_lift_start, (s32)worldmap_scene18_lift_update);
    worldmap_actor_spawn((s32)worldmap_scene_frame_start, (s32)worldmap_scene_frame_update);
    worldmap_terrain_alloc_packets();
    worldmap_effects_alloc_quads();
    worldmap_clouds_alloc_quads();
    worldmap_encounter_reset_timers();
}

/* 800837DC: Leave the world map for scene 0x269 (flag word 4). */
void worldmap_scene18_leave(void) {
    sound_stop_all_effects();
    sound_remove_effect_bank(sound_effect_bank);
    heap_free(sound_effect_bank);
    worldmap_objects_free();
    worldmap_clouds_free();
    worldmap_clouds_free_quads();
    worldmap_texture_anim_free();
    worldmap_texture_anim2_free();
    worldmap_effects_free_slots();
    worldmap_effects_free_quads();
    worldmap_terrain_free_blocks();
    heap_free(worldmap_display_buffers[0].ot);
    heap_free(worldmap_display_buffers[1].ot);
    heap_free(worldmap_display_buffers[0].packets);
    heap_free(worldmap_display_buffers[1].packets);
    heap_free(worldmap_area_data);
    worldmap_actor_free_slots();
    game_data.map = 0x269;
    game_data.entry[2] = 4;
    worldmap_next_scene_chosen = 1;
    game_data.entry[0] = worldmap_camera_angle.vy;
}

/* 800838E8: Give an actor its script. */
s32 worldmap_scene18_script_start(s32 index) {
    worldmap_actor_slots[index].u.script = worldmap_scene18_actor_script;
    return 1;
}

/* 8008390C: Start a scripted camera on the player: pitch -0x20, yaw 0x400 at distance 0x96. */
s32 worldmap_scene18_camera_start(s32 index) {
    WorldmapActor *actor;

    actor = &worldmap_actor_slots[index];
    actor->unk7C = 0x1000;
    actor->position.vx = worldmap_camera_follow_target.target.vx = worldmap_camera.target.vx = worldmap_player_position.vx;
    actor->position.vy = worldmap_camera_follow_target.target.vy = worldmap_camera.target.vy = worldmap_player_position.vy;
    actor->position.vz = worldmap_camera_follow_target.target.vz = worldmap_camera.target.vz = worldmap_player_position.vz;
    actor->state = 0;
    actor->unk4 = 0;
    actor->unk5C = 0x960000;
    worldmap_camera_angle.vx = -0x20;
    worldmap_camera_angle.vy = 0x400;
    worldmap_camera_angle.vz = 0;
    actor->motion.vx = actor->u.step = -0x20 << 12;
    worldmap_view_center_y = 0x78;
    worldmap_view_kind = 0;
    worldmap_camera_distance = 0x960000;
    actor->motion.vy = actor->unk54 = worldmap_camera_angle.vy << 12;
    actor->motion.vz = actor->unk58 = worldmap_camera_angle.vz << 12;
    return 1;
}
