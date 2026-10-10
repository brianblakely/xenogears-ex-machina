/* Battle unit from 800B7134 to 800B8098: the shattered screen's draw and
 * set-up, the battle's intro swirl and the single actions' command files
 * (800B7870-800B8068), built by the Cygnus CDK GCC 2.7.2. 800B7870's jump
 * table at 0x800709FC sits at 4 mod 8 directly after 800B3F04's odd-length
 * table at 0 mod 8, so a unit starts between the two functions (that table is
 * all of 800B3F04's unit's rodata). The screen shatter's draw (800B7134,
 * 800B7160) and the intro swirl 800B7870 share the unit's own variable
 * battle_shatter_ot, which follows 800B3F04's .bss, so the unit starts at 800B7134
 * at the latest; that is where it is placed. Its own 5-entry table is
 * followed directly by 800B8098's at 0x80070A10 (0 mod 8). */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/types.h"
#include "resident/cd.h"
#include "resident/gpu.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/sound.h"
#include "resident/sprite.h"
#include "resident/stream.h"
#include "battle/action_file.h"
#include "battle/actor.h"
#include "battle/area.h"
#include "battle/flow.h"
#include "battle/frame.h"
#include "battle/screen.h"
#include "battle/setup.h"
#include "battle/sprite.h"
#include "battle/stage.h"
#include "overlays.h"
#include "own_declarations.h"
#include "resident_views.h"
#include "sprite_effect.h"

/* This unit's functions, declared before their first use. */
void battle_shatter_draw_shards(Task *draw);
ScreenShatter *battle_shatter_cut_shards(ScreenShatter *shatter);

/* The unit's own uninitialized variable (its .bss, after
 * battle_sprite_commands.c's). */
static u32 *battle_shatter_ot; /* 800C3CB4: the ordering table the shatter draws into */

/* 800B7134: Shatter draw: into the ordering table (800B7160). */
void battle_shatter_draw(Task *draw) {
    battle_shatter_ot = (u32 *)sprite_ot;
    battle_shatter_draw_shards(draw);
}

/* 800B7160: Shatter draw: each shard that has fallen in front of the screen (z at
 * least 64), its layer's triangle turned and placed, projected at the
 * screen centre and distance 512. */
void battle_shatter_draw_shards(Task *draw) {
    ScreenShatter *shatter = draw->data;
    long offsetX;
    long offsetY;
    s32 screen;
    s32 layer;
    s32 row;
    s32 column;
    ScreenShard *shard;
    POLY_FT3 *poly;
    SVECTOR *triangle;

    ReadGeomOffset(&offsetX, &offsetY);
    screen = ReadGeomScreen();
    SetGeomOffset(160, 112);
    SetGeomScreen(512);
    for (layer = 0; layer != 2; layer++) {
        for (row = 0; row != 14; row++) {
            for (column = 0; column != 20; column++) {
                shard = &shatter->shards[layer][row][column];
                poly = &shard->poly[BATTLE_AREA.buffer];
                if (shard->position.vz >= 64) {
                    MATRIX m;
                    long p;
                    long flag;
                    s32 depth;

                    gpu_build_rotation_matrix(&shard->angles, &m);
                    TransMatrix(&m, &shard->position);
                    SetRotMatrix(&m);
                    SetTransMatrix(&m);
                    if (layer == 0) {
                        triangle = battle_shatter_upper_left_triangle;
                    } else {
                        triangle = battle_shatter_lower_right_triangle;
                    }
                    depth = RotTransPers3(&triangle[0], &triangle[1], &triangle[2],
                                          (long *)&poly->x0, (long *)&poly->x1, (long *)&poly->x2,
                                          &p, &flag) >> 6;
                    AddPrim(battle_shatter_ot + depth, poly);
                }
            }
        }
    }
    SetGeomOffset(offsetX, offsetY);
    SetGeomScreen(screen);
}

/* 800B7330: Free a heap block once drawing is done. */
void battle_free_after_drawsync(void *block) {
    DrawSync(0);
    heap_free(block);
}

/* 800B7364: Shatter destroy: end the draw task, the task and its sprites. */
void battle_shatter_destroy(ScreenShatter *shatter) {
    task_unlink_draw_node(&shatter->draw);
    task_unlink_main_node(&shatter->task);
    sprite_queue_free_later((u32)shatter);
}

/* 800B73A0: Shatter the screen copied to VRAM (0x2C0, 0x100). */
void battle_shatter_start(void) {
    battle_shatter_cut_shards((ScreenShatter *)task_alloc_two_node_task(sizeof(ScreenShatter), NULL, battle_shatter_update, battle_shatter_draw,
                                                 (void (*)(Task *))battle_shatter_destroy));
}

/* 800B73EC: Set up a shattered screen in a heap block (not run as a task). */
ScreenShatter *battle_shatter_create_in_heap(void) {
    ScreenShatter *shatter = heap_alloc(sizeof(ScreenShatter), 1);

    shatter->task.data = shatter;
    shatter->draw.data = shatter;
    return battle_shatter_cut_shards(shatter);
}

/* 800B7424: Cut the screen copied to VRAM (0x2C0, 0x100) into shards: per 16 x 16
 * cell an upper-left and a lower-right triangle, each starting further out
 * the later it moves, launched outwards at a random speed with a random spin
 * and fall. */
ScreenShatter *battle_shatter_cut_shards(ScreenShatter *shatter) {
    ScreenShard *shard;
    POLY_FT3 *poly;
    VECTOR square;
    SVECTOR angles;
    MATRIX m;
    s32 radius;
    s32 row;
    s32 layer;
    s32 column;
    s32 i;
    s32 distance;
    s32 turn;
    s32 tilt;
    s32 r;
    s32 base;
    s32 yaw;

    shatter->frame = 0;
    radius = SquareRoot0(160 * 160 + 112 * 112) << 10;
    for (layer = 0; layer != 2; layer++) {
        for (row = 0; row != 14; row++) {
            for (column = 0; column != 20; column++) {
                shard = &shatter->shards[layer][row][column];
                shard->angles.vx = 0;
                shard->angles.vy = 0;
                shard->angles.vz = 0;
                if (layer == 0) {
                    shard->position.vx = (column * 16 - 155) * 32;
                    shard->position.vy = (row * 16 - 107) * 32;
                    shard->position.vz = 0x4000;
                } else {
                    shard->position.vx = (column * 16 - 149) * 32;
                    shard->position.vy = (row * 16 - 101) * 32;
                    shard->position.vz = 0x4000;
                }
                battle_shatter_launch_velocity.vz = -500 << 16;
                battle_shatter_launch_velocity.vz = battle_shatter_launch_velocity.vz + (-(rand() % 1000) << 16);
                Square0(&shard->position, &square);
                distance = SquareRoot0(square.vx + square.vy);
                shard->delay = (radius / 32 - distance) / 2048; /* overwritten */
                shard->delay = distance / 1024;
                /* Turn outwards, a little at random; tilt by the distance. */
                turn = ratan2(shard->position.vy, shard->position.vx);
                r = rand();
                yaw = (turn += 0x600) + r % 1024;
                tilt = (distance << 11) / radius;
                r = rand();
                base = tilt - 0x20;
                tilt = base + r % 64;
                angles.vx = 0;
                angles.vy = tilt;
                angles.vz = yaw;
                RotMatrix(&angles, &m);
                ApplyMatrixLV(&m, &battle_shatter_launch_velocity, &shard->velocity);
                shard->fall = 0x70800 - ((rand() % 1600) << 8);
                shard->spin.vx = (rand() & 0xFF) - 0x7F;
                shard->spin.vy = (rand() & 0xFF) - 0x7F;
                shard->spin.vz = (rand() & 0x1FF) - 0xFF;
                for (i = 0; i != 2; i++) {
                    poly = &shard->poly[i];
                    SetPolyFT3(poly);
                    SetShadeTex(poly, 0);
                    poly->r0 = 0xFF;
                    poly->g0 = 0xFF;
                    poly->b0 = 0xFF;
                    setSemiTrans(poly, 0);
                    poly->tpage = GetTPage(2, 1, column * 16 + 0x2C0, 0x100);
                    if (layer == 0) {
                        poly->u0 = column * 16 & 0x3F;
                        poly->v0 = row * 16;
                        poly->u1 = (column * 16 & 0x3F) + 16;
                        poly->v1 = row * 16;
                        poly->u2 = column * 16 & 0x3F;
                        poly->v2 = row * 16 + 16;
                    } else {
                        poly->u0 = (column * 16 & 0x3F) + 16;
                        poly->v0 = row * 16;
                        poly->u1 = (column * 16 & 0x3F) + 16;
                        poly->v1 = row * 16 + 16;
                        poly->u2 = column * 16 & 0x3F;
                        poly->v2 = row * 16 + 16;
                    }
                }
            }
        }
    }
    return shatter;
}

/* 800B7870: The battle's intro swirl: the screen shatters (800B73EC) while the
 * battle module's set-up phases 0-2 and the scene files load, one step per
 * frame once the disc is idle, for at least 86 frames; then phase 3. The
 * screen copied to VRAM (0x2C0, 0x100) with its pixels made opaque feeds
 * the shards, and the background fades from white. */
void battle_run_intro_swirl(void) {
    RECT rect;
    u16 *pixels;
    u16 *pixel;
    s32 i;
    s32 frames;
    s32 step;
    s32 phase;
    FrameBuffer *buffer;
    BattleArea *area;
    BattleArea *frame;
    FrameBuffer *first;
    FrameBuffer *next;
    ScreenShatter *shatter;

    frames = 86;
    task_clear_lists();
    step = 1;
    phase = 0;
    pixels = heap_alloc(0x30000, 1);
    pixel = pixels;
    rect.x = 0;
    rect.y = 0;
    rect.w = 320;
    rect.h = 224;
    StoreImage(&rect, (u_long *)pixels);
    DrawSync(0);
    for (i = 0; i != 320 * 256; i++) {
        *pixel++ |= 0x8000;
    }
    rect.x = 0x2C0;
    rect.y = 0x100;
    rect.w = 320;
    rect.h = 224;
    LoadImage(&rect, (u_long *)pixels);
    DrawSync(0);
    heap_free(pixels);
    area = &BATTLE_AREA;
    buffer = &area->buffers[0];
    if (area->current == (first = buffer)) {
        buffer = &area->buffers[1];
    }
    area->current = buffer;
    area->ot = buffer->ot;
    ClearOTagR((u_long *)buffer->ot, 0x1000);
    area->buffer = 0;
    area->current = first;
    area->buffers[0].drawEnv.isbg = 1;
    area->buffers[1].drawEnv.isbg = 1;
    area->buffers[0].drawEnv.r0 = 0xFF;
    area->buffers[1].drawEnv.r0 = 0xFF;
    area->buffers[0].drawEnv.g0 = 0xFF;
    area->buffers[1].drawEnv.g0 = 0xFF;
    area->buffers[0].drawEnv.b0 = 0xFF;
    area->buffers[1].drawEnv.b0 = 0xFF;
    shatter = battle_shatter_create_in_heap();
    while (frames != 0 || step != 5) {
        if (frames > 0) {
            frames--;
        }
        frame = &BATTLE_AREA;
        next = &frame->buffers[0];
        if (frame->current == next) {
            next = &frame->buffers[1];
        }
        frame->current = next;
        frame->ot = next->ot;
        ClearOTagR((u_long *)next->ot, 0x1000);
        frame->buffer = 1 - frame->buffer;
        battle_shatter_ot = frame->ot;
        if (cd_get_pending_read_count() == 0) {
            switch (step) {
            case 0:
                break;
            case 2:
                mode_load_current_battle_stage();
                step++;
                break;
            case 1:
            case 3:
            case 4:
                battle_setup_run_phase(phase);
                phase++;
                step++;
                break;
            }
        }
        boot_check_soft_reset();
        SPAD_STACK_ENTER();
        battle_shatter_update(&shatter->task);
        battle_shatter_draw_shards(&shatter->task);
        SPAD_STACK_LEAVE();
        DrawSync(0);
        VSync(2);
        BATTLE_AREA.buffers[BATTLE_AREA.buffer].drawEnv.r0 = sprite_add_clamp_byte(BATTLE_AREA.buffers[BATTLE_AREA.buffer].drawEnv.r0, -12);
        BATTLE_AREA.buffers[BATTLE_AREA.buffer].drawEnv.g0 = sprite_add_clamp_byte(BATTLE_AREA.buffers[BATTLE_AREA.buffer].drawEnv.g0, -12);
        BATTLE_AREA.buffers[BATTLE_AREA.buffer].drawEnv.b0 = sprite_add_clamp_byte(BATTLE_AREA.buffers[BATTLE_AREA.buffer].drawEnv.b0, -12);
        PutDispEnv(&BATTLE_AREA.current->dispEnv);
        PutDrawEnv(&BATTLE_AREA.current->drawEnv);
        DrawOTag((u_long *)&BATTLE_AREA.current->ot[0xFFF]);
    }
    battle_free_after_drawsync(shatter);
    SetDispMask(0);
    cd_sync_reads(0);
    battle_setup_run_phase(3);
}

/* 800B7C28: Clear battle_unread_single_action_loaded. */
void battle_single_action_clear_loaded(void) {
    battle_unread_single_action_loaded = 0;
}

/* 800B7C34: Load single action index's command file (0x22 + 2 * index) and play its
 * stream (0x23 + 2 * index); action 0xE3 first puts back the VRAM columns
 * saved by 800B3E04. The file's first part says which gears restart: all
 * (1) or all but the acting sprite's (2). */
void battle_single_action_load(s32 index) {
    RECT rect;
    s32 file;
    s32 stream;
    s32 restart;
    Sprite *sprite;

    if (index == 0xE3) {
        rect.w = 0x40;
        rect.h = 0x100;
        rect.x = battle_gear_image_places[0].x;
        rect.y = battle_gear_image_places[0].y;
        MoveImage(&rect, 0x280, 0x100);
        rect.w = 0x40;
        rect.h = 0x100;
        rect.x = battle_gear_image_places[1].x;
        rect.y = battle_gear_image_places[1].y;
        MoveImage(&rect, 0x240, 0x100);
        rect.w = 0x40;
        rect.h = 0x100;
        rect.x = battle_gear_image_places[2].x;
        rect.y = battle_gear_image_places[2].y;
        MoveImage(&rect, 0x200, 0x100);
        DrawSync(0);
    }
    battle_stop_reads_finish_loads();
    cd_select_directory(0xC, 2);
    file = index * 2 + 0x22;
    stream = index * 2 + 0x23;
    battle_unread_command_file_loaded = 1;
    battle_unread_single_action_loaded = 1;
    mode_battle_action_command_file = heap_alloc(cd_get_aligned_file_size(file), 0);
    cd_read_file(file, mode_battle_action_command_file, 0, 0x80);
    battle_wait_for_disc();
    restart = (*(u16 *)(mode_battle_action_command_file[1] + (s32)mode_battle_action_command_file) >> 12) & 3;
    if (restart != 0) {
        battle_gear_restart_mode = restart;
        sprite = battle_acting_sprite;
        battle_camera_start_move(0);
        if (restart == 1) {
            battle_end_party_sprites(-1);
            battle_gear_image_places_taken = 0;
        } else {
            battle_gear_image_places_taken &= ~(1 << SPRITE_SLOT(sprite));
            battle_end_party_sprites(SPRITE_SLOT(sprite));
        }
    }
    mode_battle_action_stream_ring = stream_create_ring(8, 0);
    if (cd_get_aligned_file_size(stream) > 0x10) {
        stream_start_image_load(stream, mode_battle_action_stream_ring, 0, 0, 0, 0, 0, 0, 0, 0);
    }
    battle_wait_for_disc();
    DrawSync(0);
    battle_command_file = mode_battle_action_command_file;
    heap_free(mode_battle_action_stream_ring);
}

/* 800B7E94: Start the loaded command file for the acting sprite: upload its images
 * and, when its frames take their image from the sequencer, give the
 * sprite an effect sprite running its command motion (else the file becomes
 * the sprite's own resource); start its sound bank. 1 when the sprite runs
 * it itself. */
u8 battle_single_action_start(void) {
    VramPoint at;
    VramPoint clut;
    Task wait;
    SpriteSource saved;
    Sprite *actor;
    Sprite *runner;
    s32 own;
    SpriteSource *resource;

    actor = battle_acting_sprite;
    task_link_main_node(0, &wait);
    wait.update = NULL;
    battle_wait_for_disc();
    resource = (SpriteSource *)sprite_effect_source;
    at.x = 0x380;
    at.y = 0x100;
    clut.x = 0;
    clut.y = 0x1F4;
    battle_sprite_script_finished = 0;
    sprite_resolve_resource(resource, mode_battle_action_command_file, at, clut, 0);
    own = 0;
    battle_load_module();
    if (sprite_is_cell_directory((u8 *)resource->frames)) {
        saved = *(SpriteSource *)battle_acting_sprite->image;
        runner = sprite_create_child(battle_acting_sprite, (void *)(resource->animations[battle_acting_sprite_command_motion + 1] + (s32)resource->animations), resource);
    } else {
        own = 1;
        runner = battle_acting_sprite;
        sprite_set_alternate_resource(runner, (s32)mode_battle_action_command_file);
        sprite_start_animation(runner, -1);
    }
    actor->word50 = runner->word50 = (s32)battle_install_command_file_parts(mode_battle_action_command_file);
    battle_command_file_started = 1;
    battle_music_lowered = 1;
    sound_set_seq_fade((SoundSeq *)battle_music_seq, 0x60, 0x78);
    task_unlink_main_node(&wait);
    return own;
}

/* 800B8048: Set the acting sprite of a single action. */
void battle_single_action_set_actor(Sprite *sprite) {
    battle_acting_sprite = sprite;
}

/* 800B8054: Request single action `action`; the frame loop runs it (800B8068). */
void battle_single_action_request(s32 action) {
    sprite_single_action_request = action;
    sprite_single_action_done = 0;
}

/* 800B8068: Run a requested single action: load its command file (800B7C34) and
 * start it (800B7E94); mark it done. */
void battle_single_action_run(s32 action) {
    battle_single_action_load(action);
    battle_single_action_start();
    sprite_single_action_done = 1;
}
