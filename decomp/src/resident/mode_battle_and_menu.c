/* Battle-mode set-up and menu-mode support (8001b844-8001c76c), GCC 2.6.3
 * at -G8 (8001bf38 and 8001c1a8 match only so; its rodata opens at
 * 0x8001833c): the battle's display buffers and projection, its saved names,
 * sound programs and files, a random byte, then the menu's display, input,
 * frame and body and the menu mode's entry (mode 5). It addresses its own
 * small commons through $gp, other units' small globals and every address
 * absolutely (EXTERN_mode_battle_and_menu). The battle effect-script table that
 * follows in .text (8001c76c-8001c8dc) stays original data, and the sprite
 * unit, built by another compiler, starts after it. */
#include "common.h"
#include "psyq/libc.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "battle/area.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/menu.h"
#include "resident/mode.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/sprite.h"

/* Sound programs requested for each battle mode; 0xff absent. */
u8 mode_battle_sound_programs[6][3] = { /* 8004F388 */
    {1, 2, 0xFF}, {3, 4, 0xFF}, {5, 6, 7}, {8, 9, 10}, {1, 2, 0xFF}, {1, 2, 0xFF},
};

/* Menu screen names, in 256-byte buffers. */
char menu_state_screen_name_buffers[7][0x100] = { /* 8004F39C */
    "Normal Menu", "Member Change", "Load Game", "Enter Name", "Shop", "Robo", "CD Change",
};
char *menu_state_screen_names[7] = { /* 8004FA9C */
    menu_state_screen_name_buffers[0], menu_state_screen_name_buffers[1], menu_state_screen_name_buffers[2], menu_state_screen_name_buffers[3],
    menu_state_screen_name_buffers[4], menu_state_screen_name_buffers[5], menu_state_screen_name_buffers[6],
};

/* Stripped debug hooks share this no-op entry. Their old argument lists
 * and forwarded return register remain part of the calling sequence, so the
 * calls here are K&R ones of an s32 function (its definition in
 * console_and_sound_driver.c is void (void)). */
s32 console_empty_debug_hook();
void mode_battle_init_draw_env(DRAWENV *env);

/* 8001B844: Initialize the battle's 320x224 double buffer (the battle area's two
 * frame buffers) and GTE projection. Each draw environment is reached from
 * the display environment after it. */
void mode_battle_init_display(void) {
    DISPENV *disp;
    DRAWENV *draw;
    DRAWENV *otherDraw;

    ResetGraph(1);
    console_empty_debug_hook(0x300, 0);
    console_empty_debug_hook(console_empty_debug_hook(8, 0x10, 0x140, 0xF0, 0, 0x1000));
    InitGeom();
    SetGeomOffset(0xA0, 0xB4);
    SetGeomScreen(0x200);
    disp = &battle_area.buffers[0].dispEnv;
    SetDefDispEnv(disp, 0, 0xE0, 0x140, 0xE0);
    draw = (DRAWENV *)((u8 *)disp - sizeof(DRAWENV));
    SetDefDrawEnv(draw, 0, 0, 0x140, 0xE0);
    SetDefDispEnv((DISPENV *)((u8 *)disp + sizeof(FrameBuffer)), 0, 0, 0x140, 0xE0);
    otherDraw = (DRAWENV *)((u8 *)disp + sizeof(FrameBuffer) - sizeof(DRAWENV));
    SetDefDrawEnv(otherDraw, 0, 0xE0, 0x140, 0xE0);
    mode_battle_init_draw_env(draw);
    mode_battle_init_draw_env(otherDraw);
}

/* 8001B94C: Battle draw environment: clear the background to (0x3c, 0x78, 0x78) with dithering. */
void mode_battle_init_draw_env(DRAWENV *env) {
    env->isbg = 1;
    env->dtd = 1;
    env->r0 = 0x3C;
    env->g0 = 0x78;
    env->b0 = 0x78;
}

/* The second byte of the saved name slots: 8001b970 reads them from a base
 * of their own, which game_data.names does not compile to (link.ld). */
extern u8 game_data_name_code_high_bytes[];
/* The battle script variables. 8001b970 clears twenty halfwords back from
 * [19]: the 16 variables and the first 8 bytes of the sound driver's SPU
 * attributes sound_volumes that follow them. The battle reads them signed, so
 * the shared headers leave them out. */
extern u16 mode_battle_ai_variables[];
u8 menu_state_saved_cursor;
u8 mode_pending_battle_formation; /* the next battle's formation + 1 (resident/mode.h) */
/* Callers in other targets declare these two differently. The resident's
 * own prototypes (own_declarations.h) are not included here: its window.h
 * declares the window colour as the array this unit cannot. */
void text_decode_codes(u16 *codes, u8 *out, u32 count);
void sound_play_effect_on_last_channels(s32 effect);

/* 8001B970: Load directory 16 file 3 into the saved game data, decode the first
 * 31 twenty-byte name slots, and clear the battle script variables. */
void mode_load_initial_game_data(void) {
    u16 codes[12]; /* 24-byte workspace; at most ten codes per name */
    u8 decoded[20];
    void *file;
    u16 *counter;
    s32 slot;
    s32 i;
    u8 *low;
    u8 *high;

    cd_select_directory(16, 0);
    heap_select_owner_tag(2, 0);
    file = heap_alloc(cd_get_aligned_file_size(3), 1);
    cd_read_file(3, file, 0, 0x80);
    cd_sync_reads(0);
    memmove(&game_data, file, 0x2358);
    heap_free(file);
    for (slot = 0; slot < 31; slot++) {
        for (i = 0; i < 20; i += 2) {
            low = (u8 *)codes;
            high = low + 1;
            low[i] = game_data.names[slot][i];
            high[i] = game_data_name_code_high_bytes[slot * 20 + i];
            if (game_data.names[slot][i] == 0xF && game_data_name_code_high_bytes[slot * 20 + i] == 0) {
                break;
            }
        }
        text_decode_codes(codes, decoded, i / 2);
        for (i = 0; i < 20; i++) {
            game_data.names[slot][i] = decoded[i];
        }
    }
    for (i = 19, counter = &mode_battle_ai_variables[19]; i >= 0; i--) {
        *counter-- = 0;
    }
    menu_state_saved_cursor = 6;
    mode_pending_battle_formation = 0;
}

/* 8001BB0C: Load the battle stage that the formation's byte 2 (8006f9de) names (800379d8). */
void mode_load_current_battle_stage(void) {
    mode_load_battle_stage(formation_active.stage, 0, &mode_battle_stage_file, &mode_battle_stage_unused_word, &mode_battle_scene_file);
}

u8 mode_battle_standalone;
u8 mode_unread_battle_setup_byte;
/* The window colour is one object, the u8[3] the overlays declare: under
 * the slot rule no other variable shares its word. ASPSX 2.34 addressed a
 * common at an offset absolutely, so only [0] went through $gp (maspsx
 * models that), but GNU as moves a small common's offset accesses to $gp
 * as well, so the array cannot be declared here and its last two bytes are
 * names of their own (link.ld). */
u8 window_color;
extern u8 window_color_green;
extern u8 window_color_blue;
s32 window_semi_transparency_mode;

/* 8001BB50: Set the battle setup flags, initialize battle setup data, wait for disc I/O,
 * then set the window colour (0x88, 0x76, 0x54) and the windows'
 * semi-transparency mode (2). */
void mode_init_game_data(void) {
    mode_battle_standalone = 1;
    mode_unread_battle_setup_byte = 0;
    mode_load_initial_game_data();
    cd_sync_reads(0);
    window_color = 0x88;
    window_color_green = 0x76;
    window_color_blue = 0x54;
    window_semi_transparency_mode = 2;
}

/* This unit's small commons, addressed through $gp; they merge with
 * commons/commons_small.c's definitions. */
void *mode_battle_heap_marker; /* heap marker for the high-memory reservation */
void *mode_battle_heap_reservation; /* reservation below the heap marker */
SoundBank *mode_battle_effect_bank;
void *mode_battle_setup_archive;
u8 mode_battle_kind;

/* 8001BBAC: Reserve high memory, load a sound bank and files 3 and 4, then request
 * the mode's three optional sound programs. */
void mode_battle_load_files(void) {
    s32 i;
    u8 program;
    void *overlay;
    void *file;

    heap_select_owner_tag(2, 0);
    cd_select_directory(12, 0);
    mode_battle_heap_marker = heap_alloc(4, 1);
    mode_battle_heap_reservation = heap_alloc((u32)mode_battle_heap_marker - 0x801E4000U, 1);
    overlay = (void *)0x801E4000;
    mode_battle_effect_bank = heap_alloc(cd_get_aligned_file_size(2), 1);
    file = heap_alloc(cd_get_aligned_file_size(3), 1);
    mode_battle_file_list[0].file = 2;
    mode_battle_file_list[1].file = 3;
    mode_battle_setup_archive = file;
    mode_battle_file_list[1].destination = file;
    mode_battle_file_list[2].file = 4;
    mode_battle_file_list[2].destination = overlay;
    mode_battle_file_list[3].file = 0;
    mode_battle_file_list[3].destination = NULL;
    mode_battle_file_list[0].destination = mode_battle_effect_bank;
    cd_read_file_list(mode_battle_file_list, 0, 0x80);
    while (cd_get_pending_read_count() == 3) {
    }
    sound_add_effect_bank(mode_battle_effect_bank);
    if (mode_battle_kind != 4) {
        for (i = 0; i < 3; i++) {
            program = ((u8 *)mode_battle_sound_programs)[mode_battle_kind * 3 + i];
            if (program != 0xFF) {
                sound_play_effect_on_last_channels(((u32)mode_battle_effect_bank->id << 16) | program);
            }
        }
    }
}

/* 8001BD40: A random byte from the requested range. Low 0xff is returned unchanged;
 * otherwise high 0 returns 0 and equal bounds return low. A span of 0xff
 * uses the whole random byte; smaller signed spans use modulo span + 1. */
u8 mode_get_random_byte_in_range(u8 low, u8 high) {
    s32 span;

    if (low != 0xFF) {
        if (high == 0) {
            return 0;
        }
        if (low == high) {
            return low;
        }
        span = high - low;
        if (span < 0xFF) {
            return low + (u8)rand() % (span + 1);
        }
        return rand();
    }
    return low;
}

/* 8001BDDC: Menu buffer: no background clear, dithering, and a 256x216 display area 10 lines down. */
void menu_state_init_buffer(MenuBuffer *buffer) {
    buffer->draw.dtd = 1;
    buffer->draw.isbg = 0;
    buffer->draw.r0 = 0;
    buffer->draw.g0 = 0;
    buffer->draw.b0 = 0;
    buffer->disp.screen.x = 0;
    buffer->disp.screen.y = 0xA;
    buffer->disp.screen.w = 0x100;
    buffer->disp.screen.h = 0xD8;
}

/* 8001BE14: Menu display set-up: GTE projection and the two 320x224 buffers. */
void menu_state_init_display(void) {
    SetGeomOffset(0xA0, 0x70);
    SetGeomScreen(0x200);
    SetDefDispEnv(&menu_state_current->buffers[0].disp, 0, 0xE0, 0x140, 0xE0);
    SetDefDrawEnv(&menu_state_current->buffers[0].draw, 0, 0, 0x140, 0xE0);
    SetDefDispEnv(&menu_state_current->buffers[1].disp, 0, 0, 0x140, 0xE0);
    SetDefDrawEnv(&menu_state_current->buffers[1].draw, 0, 0xE0, 0x140, 0xE0);
    menu_state_init_buffer(&menu_state_current->buffers[0]);
    menu_state_init_buffer(&menu_state_current->buffers[1]);
}

/* 8001BEEC: Reset the menu's two views to the origin at distance 0x800. */
void menu_state_reset_views(void) {
    MenuState *work = menu_state_current;

    work->offset.vz = 0x800;
    work->offset2.vz = 0x800;
    work->angles.vx = work->angles.vy = work->angles.vz = 0;
    work->offset.vx = work->offset.vy = 0;
    work->angles2.vx = work->angles2.vy = work->angles2.vz = 0;
    work->offset2.vx = work->offset2.vy = 0;
    work->word2e8 = 1;
    work->view_motion = 0;
}

/* 8001BF38: Decode the menu input of this frame: directions 0-3, confirm 4, debug toggles; 8 when nothing applies. */
void menu_state_decode_input(void) {
    s32 input = 8;
    u16 buttons;

    if (pad_has_queue_overflowed() != 0) {
        pad_clear_queue();
    } else {
        while (pad_dequeue_state() != 0) {
            buttons = pad_port0_repeated;
            if (buttons & 0x2000) {
                input = 0;
                break;
            }
            if (buttons & 0x4000) {
                input = 1;
                break;
            }
            if (buttons & 0x8000) {
                input = 2;
                break;
            }
            if (buttons & 0x1000) {
                input = 3;
                break;
            }
            if (buttons & 0x20) {
                input = 4;
                break;
            }
            if (buttons & 0x100) {
                menu_state_current->debug_show = menu_state_current->debug_show == 0;
                input = 0xC;
                break;
            }
            if (buttons & 0x4) {
                if (menu_state_current->debug_value != 0) {
                    menu_state_current->debug_value--;
                }
                break;
            }
            if (buttons & 0x1) {
                menu_state_current->debug_value++;
                break;
            }
        }
    }
    menu_state_current->input = input;
}

/* 8001C074: Menu frame: decode input, flip buffers, clear the ordering table, draw the debug overlays, then present. */
void menu_state_update_frame(void) {
    MenuState *work;

    menu_state_decode_input();
    work = menu_state_current;
    work->current = work->current == &work->buffers[0] ? &work->buffers[1] : &work->buffers[0];
    work->buffer_index = work->buffer_index == 0;
    ClearOTagR(work->current->ot, 16);
    if (*mode_disc_mode_pointer != -1) {
        if (menu_state_current->debug_show) {
            heap_print_report(3, menu_state_current->debug_value, 0xF, 0x80AC);
        }
        if (*mode_disc_mode_pointer != -1) {
            console_flush(menu_state_current->current->ot);
        }
    }
    DrawSync(0);
    VSync(0);
    PutDrawEnv(&menu_state_current->current->draw);
    PutDispEnv(&menu_state_current->current->disp);
    DrawOTag(&menu_state_current->current->ot[15]);
}

/* 8001C1A8: Menu mode body: with the debug start, pick the menu screen and its parameter on screen and load the menu overlay (plus the extra blocks screen 5 needs); then run the chosen screen and, after a debug start, release everything and dispatch. */
void menu_state_run_screen(void) {
    s32 screen;
    s32 number;
    u8 running;
    void *low;
    void *overlay;

    screen = 0;
    number = 0;
    running = 1;
    if (menu_state_debug_start != 0) {
        do {
            console_printf("\n\n     Menu=(%d)%s\n", screen, menu_state_screen_names[screen]);
            if (screen < 4) {
                if (number < 11) {
                    console_printf("\n\n     Chr =(%d)\n", number);
                } else {
                    console_printf("\n\n     Robo =(%d)\n", number - 11);
                }
            } else if (screen != 6) {
                console_printf("\n\n     ShopNo =(%d)\n", number);
            } else {
                console_printf("\n\n     CdNo =(%d)\n", number);
            }
            switch (menu_state_current->input) {
            case 4:
                running = 0;
                break;
            case 0:
                screen++;
                number = 0;
                if (screen >= 7) {
                    screen = 0;
                }
                break;
            case 2:
                screen--;
                number = 0;
                if (screen < 0) {
                    screen = 6;
                }
                break;
            case 3:
                if (screen < 4) {
                    if (++number >= 31) {
                        number = 0;
                    }
                } else if (screen != 6) {
                    number++;
                } else {
                    number = number == 0;
                }
                break;
            case 1:
                if (--number < 0) {
                    if (screen < 4) {
                        number = 30;
                    } else if (screen != 6) {
                        number = 0xFF;
                    } else {
                        number = number == 0;
                    }
                }
                break;
            }
            menu_state_update_frame();
        } while (running);
        menu_state_screen = screen;
        menu_state_screen_parameter = number;
        menu_state_current->buffers[0].draw.isbg = 0;
        menu_state_current->buffers[1].draw.isbg = 0;
        SetDispMask(0);
        menu_state_current->current = &menu_state_current->buffers[1];
        SetDispMask(1);
    }
    cd_select_directory(0x10, 0);
    if (menu_state_debug_start != 0) {
        game_data.gold = 999999999;
        heap_select_owner_tag(2, 0);
        menu_state_resource_file = heap_alloc(cd_get_aligned_file_size(1), 0);
        cd_read_file(1, menu_state_resource_file, 0, 0x80);
        cd_sync_reads(0);
        if (menu_state_screen == 5) {
            cd_select_directory(4, 0);
            menu_state_debug_heap_marker = heap_alloc(4, 1);
            menu_state_debug_heap_reservation = heap_alloc((u8 *)menu_state_debug_heap_marker - (u8 *)0x801DC000, 1);
            cd_read_file(0x6B9, (void *)0x801DC000, 0, 0x80);
            cd_sync_reads(0);
            cd_select_directory(0x10, 0);
            menu_state_big_ots[0] = heap_alloc(0x4000, 0);
            menu_state_big_ots[1] = heap_alloc(0x4000, 0);
        }
        low = heap_alloc(4, 1);
        overlay = heap_alloc((u8 *)low - (u8 *)0x801C5000, 1);
        cd_read_file(menu_state_screen + 5, (void *)0x801C5000, 0, 0x80);
        cd_sync_reads(0);
    }
    cd_select_directory(0x10, 0);
    switch (menu_state_screen) {
    case 0:
        menu_main();
        break;
    case 1:
        member_change_main();
        break;
    case 3:
        name_entry_main();
        break;
    case 4:
        item_shop_main();
        break;
    case 2:
    case 6:
        menu_main();
        mode_select_next_mode(1);
        break;
    case 5:
        gear_shop_main();
        break;
    }
    if (menu_state_debug_start != 0) {
        heap_free(low);
        heap_free(overlay);
        if (menu_state_screen == 5) {
            heap_free(menu_state_debug_heap_marker);
            heap_free(menu_state_debug_heap_reservation);
            heap_free(menu_state_big_ots[0]);
            heap_free(menu_state_big_ots[1]);
        }
        menu_state_debug_start = 1;
        mode_dispatch(0);
    }
}

/* 8001C634: Mode 5, the menu: allocate and clear its work block, set up the display, then run the menu body. */
void mode_run_menu(void) {
    menu_state_current = heap_alloc(0x1E98, 0);
    bzero((u8 *)menu_state_current, 0x1E98);
    menu_state_current->input = 8;
    heap_select_owner_tag(2, 0);
    menu_state_current->current = &menu_state_current->buffers[1];
    menu_state_current->debug_show = 0;
    menu_state_current->debug_value = 1;
    menu_state_current->frame_counter = 0;
    menu_state_current->drawing = 0;
    menu_state_init_display();
    if (menu_state_debug_start != 0) {
        menu_state_current->buffers[0].draw.isbg = 1;
        menu_state_current->buffers[1].draw.isbg = 1;
    }
    menu_state_reset_views();
    VSync(0);
    PutDrawEnv(&menu_state_current->buffers[0].draw);
    PutDrawEnv(&menu_state_current->buffers[1].draw);
    PutDispEnv(&menu_state_current->buffers[0].disp);
    PutDispEnv(&menu_state_current->buffers[1].disp);
    SetDispMask(1);
    menu_state_run_screen();
    menu_state_debug_start = 1;
}
