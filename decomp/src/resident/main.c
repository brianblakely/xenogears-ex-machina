/* Resident startup, mode dispatcher and kernel menu (80019524-8001b6c4),
 * GCC 2.7.2 at -G0 (8001a344 matches only under 2.7.2): the handwritten
 * entry, stack set-up and BSS clear (boot_entry_point.s, boot_reset_stack_and_gp.s,
 * boot_clear_bss_range.s), the boot, the mode table and dispatcher, the core dump,
 * reset and shutdown, the boot logo, the fatal error screen, the kernel menu
 * (mode 0) with its Game of Life debug screen, the game-wide state reset, the
 * party file and map loaders and the music release. Its rodata opens the
 * program's after the disc data (0x80018080); its variables open the .sbss
 * (800592bc) and the larger ones the .bss (800595e8). It ends at the
 * battle-mode entry, a -G8 unit that addresses its own small common through
 * $gp (battle_mode.c). */
#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libcd.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/libsn.h"
#include "psyq/libspu.h"
#include "resident/cd.h"
#include "resident/console.h"
#include "resident/gamedata.h"
#include "resident/heap.h"
#include "resident/mode.h"
#include "resident/model.h"
#include "resident/pad.h"
#include "resident/sound.h"
#include "resident/text.h"
#include "own_declarations.h"

/* This unit's own variables. GCC emits them after the code, and the
 * original assembler gave those of up to 8 bytes the unit's .sbss, ahead of
 * every other unit's (800592bc), and the larger kernel menu buffers its .bss
 * (800595e8; SBSS_main in slus_006.64.mk). */
static void *mode_overlay_block;                      /* 800592BC: the loaded mode block */
static s32 mode_overlay_block_mode;                   /* 800592C0: the mode whose block is loaded, or -1 */
static s32 mode_kernel_menu_frame_count;              /* 800592C4: kernel menu frame count */
static s32 mode_kernel_menu_buffer_index;             /* 800592C8: kernel menu buffer index */
static KernelBuffer *mode_kernel_menu_current_buffer; /* 800592CC: kernel menu current buffer */
static s32 mode_kernel_menu_running;                  /* 800592D0: kernel menu running */
static u8 *mode_game_of_life_cells;                   /* 800592D4 */
static u8 *mode_game_of_life_neighbor_counts;         /* 800592D8 */
static LifeTile *mode_game_of_life_tiles[2];          /* 800592DC: tile buffers per display buffer */
static KernelBuffer mode_kernel_menu_buffers[2];      /* 800595E8: kernel menu buffers */

/* Each mode's overlay file in directory 1; the kernel menu (mode 0) has none. */
s32 mode_overlay_files[] = {0, 0xE, 0x10, 0xF, 0xD, 0x11, 0x12}; /* 8004EAA0 */

/* The packed boot logo (80019d48): its unpacked size, a 6209-byte 16-colour
 * TIM, and the LZSS stream 80032e88 decodes. */
INCLUDE_ASSET(".data", boot_packed_logo, 0x8004EABC, 0x800);

/* 80019524 */
INCLUDE_ASM("decomp/src/resident", boot_entry_point);

/* 80019548 */
INCLUDE_ASM("decomp/src/resident", boot_reset_stack_and_gp);

/* 80019560 */
INCLUDE_ASM("decomp/src/resident", boot_clear_bss_range);

/* 80019578: Boot: initialise the system libraries, the disc index and the heap,
 * load and install the resident data files, then enter the first mode. */
void boot_main(void) {
    RECT screen;
    void *file2;
    void *file3;
    void *file4;
    void *file5;
    void *data;
    s32 tag;

    ResetCallback();
    SetGraphDebug(0);
    SetVideoMode(0);
    ResetGraph(0);
    screen.x = 0;
    screen.y = 0;
    screen.w = 0x180;
    screen.h = 0x1E0;
    ClearImage(&screen, 0, 0, 0);
    DrawSync(0);
    SetDispMask(1);
    InitGeom();
    pad_start_controllers();
    InitCARD(1);
    StartCARD();
    _bu_init();
    VSyncCallback(pad_vblank_callback);
    heap_init((HeapHeader *)model_get_overlay_area(), (u8 *)0x801FC000);
    SpuInit();
    cd_init_disc_access(cd_disc_files, cd_disc_directories, mode_disc_mode);
    sound_start_driver(0);
    cd_select_directory(0, 1);

    file2 = heap_alloc(cd_get_file_size(2), 0);
    file3 = heap_alloc(cd_get_file_size(3), 0);
    file4 = heap_alloc(cd_get_file_size(4), 0);
    file5 = heap_alloc(cd_get_file_size(5), 0);
    cd_read_file(2, file2, 0, 0);
    cd_read_file(3, file3, 0, 0);
    cd_read_file(4, file4, 0, 0);
    cd_read_file(5, file5, 0, 0);
    cd_sync_reads(0);
    sound_load_wave_bank(file2, 0);
    mode_shared_wave_bank = sound_load_wave_bank(file3, 0);
    sound_load_wave_bank(file4, 0);
    mode_wave_bank_5 = sound_load_wave_bank(file5, 0);

    tag = heap_get_owner_tag();
    heap_set_owner_tag(6);
    data = heap_alloc(cd_get_file_size(6), 0);
    cd_read_file(6, data, 0, 0);
    cd_sync_reads(0);
    heap_set_next_class(0x30);
    text_install_font(text_unpack_lzss_alloc(data, 1));
    heap_free(data);
    data = heap_alloc(cd_get_file_size(7), 0);
    cd_read_file(7, data, 0, 0);
    cd_sync_reads(0);
    heap_set_next_class(0x31);
    text_install_system_data(text_unpack_lzss_alloc(data, 1));
    heap_free(data);
    heap_set_owner_tag(tag);

    sound_sync_transfer(0x10);
    heap_free(file2);
    heap_free(file3);
    heap_free(file4);
    heap_free(file5);
    mode_reset_game_state();
    mode_init_game_data();
    sprite_reset_engine();
    mode_set_arena_task(0);
    mode_overlay_block_mode = -1;
    mode_overlay_block = NULL;
    cd_movie_request_kind = 1;
    cd_movie_request_next_mode = 1;
    cd_movie_request_unskippable = 0;
    if (cd_get_disc_number() == 1) {
        cd_movie_request_index = 0x10;
    } else {
        cd_movie_request_index = 7;
    }
    if (pad_get_controller_kind(0) != 0) {
        while (pad_port0_held == 0x90C) {
            pad_dequeue_state();
        }
    }
    boot_show_logo();
    boot_empty_step();
    mode_select_next_mode(6);
    mode_dispatch(0);
}

/* 80019964: An empty entry: a stripped debug output (the menu passes it strings). */
void mode_empty_debug_print(void) {
}

/* 8001996C: Select the next mode; a different mode releases the cached mode block. */
void mode_select_next_mode(s32 mode) {
    mode_next_mode = mode;
    if (mode != mode_overlay_block_mode) {
        if (mode_overlay_block != NULL) {
            heap_free(mode_overlay_block);
            mode_overlay_block = NULL;
        }
        mode_overlay_block_mode = -1;
    }
}

/* 800199CC: Load the mode's overlay file into a heap block (tag 6, from the top,
 * quietly) unless it is already cached; returns the block. */
void *mode_load_overlay_block(s32 mode) {
    s32 tag;
    s32 quiet;
    s32 base;
    s32 index;

    if (mode_overlay_block_mode != mode) {
        mode_overlay_block_mode = mode;
        tag = heap_get_owner_tag();
        cd_get_selected_directory(&base, &index);
        heap_set_owner_tag(6);
        cd_select_directory(0, 1);
        quiet = heap_set_quiet_failures(1);
        mode_overlay_block = heap_alloc(cd_get_file_size(mode_overlay_files[mode]), 1);
        if (mode_overlay_block != NULL) {
            cd_read_file(mode_overlay_files[mode], mode_overlay_block, 0, 0);
        } else {
            mode_overlay_block_mode = -1;
        }
        heap_set_quiet_failures(quiet);
        cd_select_directory(base, index);
        heap_set_owner_tag(tag);
    }
    return mode_overlay_block;
}

/* Unreferenced. */
const s32 mode_unreferenced_rodata_word = 0; /* 80018080 */

/* Where a mode's overlay block is decoded. */
u8 *const mode_overlay_decode_destination = mode_overlay_area; /* 80018084 */

/* 80019ACC: Mode dispatcher: report a fatal error (with the caller) if given, reset
 * graphics and the heap, clear the next mode's BSS, load its overlay, then run it
 * and dispatch again. */
void mode_dispatch(s32 error) {
    ModeEntry *mode;
    void *block;
    u32 unused[2]; /* unused in the original; reserves 8 bytes */
    u32 caller;

    if (error != 0) {
        GET_RA(&caller);
        mode_show_fatal_error(error, caller);
    }
    mode = &mode_table[mode_next_mode];
    ResetGraph(1);
    DrawSyncCallback(0);
    pad_set_vblank_hook(0);
    DrawSync(0);
    VSync(2);
    heap_move_start((HeapHeader *)(mode->bss_end + 0x800));
    mode_select_default_heap_tag();
    if (mode->loaded) {
        boot_clear_bss_range(mode->bss_start, mode->bss_end);
        block = mode_load_overlay_block(mode_next_mode);
        cd_sync_reads(0);
        text_unpack_lzss(block, mode_overlay_decode_destination);
        DrawSync(0);
        VSync(0);
        EnterCriticalSection();
        DrawSync(0);
        VSync(0);
        FlushCache();
        ExitCriticalSection();
    }
    boot_reset_stack_and_gp();
    heap_move_start((HeapHeader *)(mode->bss_end + 4));
    heap_reset_defaults();
    pad_clear_queue();
    mode_select_next_mode(0);
    mode->entry();
    mode_dispatch(0);
}

/* The next mode: mode_select_next_mode writes it and the dispatcher reads it, yet the
 * original keeps it among this unit's read-only data, between the decode
 * destination and the mode table. */
s32 mode_next_mode __attribute__((section(".rodata"))) = 0; /* 80018088 */

/* Each mode's entry, the BSS the dispatcher clears before it (the words
 * after bss_start through bss_end) and whether its overlay file
 * (mode_overlay_files) is first decoded to 0x8006faf0. Modes 0 (the kernel menu),
 * 2 and 5 enter resident code; 0 and 5 name the resident's own BSS bounds,
 * those the entry point clears. The others enter their overlay image,
 * linked apart: field, world map, menu and movie. */
ModeEntry mode_table[] __attribute__((section(".rodata"))) = { /* 8001808C */
    { mode_run_kernel_menu, (u8 *)&heap_report_output, boot_bss_last_word, 0 },
    { (void (*)(void))0x80077E88, (u8 *)0x800AF5E4, (u8 *)0x800C426C, 1 },
    { mode_run_battle, (u8 *)0x800C3A6C, (u8 *)0x800D39F0, 1 },
    { (void (*)(void))0x80070CFC, (u8 *)0x8009BBB0, (u8 *)0x8009D80C, 1 },
    { (void (*)(void))0x80088E90, (u8 *)0x800925D0, (u8 *)0x8009B554, 1 },
    { mode_run_menu, (u8 *)&heap_report_output, boot_bss_last_word, 0 },
    { (void (*)(void))0x800737EC, (u8 *)0x80076F38, (u8 *)0x80077454, 1 },
};

/* 80019C2C: Write main RAM (2 MiB) to the development PC as c:\core. */
void mode_write_core_dump(void) {
    s32 fd;

    PCinit();
    fd = PCcreat("c:\\core", 0);
    PCwrite(fd, (void *)0x80000000, 0x200000);
    PCclose(fd);
}

/* Fatal errors shown by 80019ef8: their count and the messages of the kernel
 * and heap errors 0x80-0x85. */
s32 mode_fatal_error_count = 0; /* 8004F2BC */
char *mode_fatal_error_messages[] = { /* 8004F2C0 */
    "LsKernel:Program Not Defined",
    "LsKernel:PC File Not Found",
    "LsGetMem:Memory Not Enough",
    "LsFreeMem:Can't Release NULL Pointer",
    "LsGetMem:MCB Broken",
    "LsFreeMem:This ptr isn't MCB",
};

/* 80019C7C: Select heap owner tag 10 (clearing its word and the quiet flag). */
void mode_select_default_heap_tag(void) {
    heap_select_owner_tag(10, 0);
}

/* 80019CA0: Soft reset while the reset button combination is held. */
void boot_check_soft_reset(void) {
    if (pad_port0_held == 0x90C) {
        boot_restart();
    }
}

/* 80019CD0: Shut down the libraries and restart from the entry point. */
void boot_restart(void) {
    SwExitCriticalSection();
    ResetGraph(0);
    cd_shutdown_disc_access();
    sound_stop_driver();
    SpuQuit();
    pad_set_vblank_hook(0);
    DrawSyncCallback(0);
    VSyncCallback(NULL);
    CdFlush();
    StopPAD();
    SwEnterCriticalSection();
    boot_entry_point();
}

/* 80019D48: Boot logo: upload the logo image and its palette, then fade the logo
 * sprite in, hold it and fade it out. */
void boot_show_logo(void) {
    DRAWENV draw;
    DISPENV disp;
    SPRT logo;
    RECT rect;
    u8 *image;
    s32 level;
    s32 frame;

    heap_set_owner_tag(6);
    image = text_unpack_lzss_alloc(boot_packed_logo, 1);
    rect.x = 0;
    rect.y = 0xF0;
    rect.w = 0x10;
    rect.h = 1;
    LoadImage(&rect, (u_long *)(image + 0x14));
    rect.x = 0x280;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x30;
    LoadImage(&rect, (u_long *)(image + 0x40));
    setSprt(&logo);
    logo.x0 = 0x20;
    logo.y0 = 0x58;
    logo.v0 = 0;
    logo.u0 = 0;
    logo.w = 0x100;
    logo.h = 0x30;
    logo.clut = GetClut(0, 0xF0);
    SetDefDrawEnv(&draw, 0, 0, 0x140, 0xE0);
    SetDefDispEnv(&disp, 0, 0, 0x140, 0xE0);
    PutDrawEnv(&draw);
    PutDispEnv(&disp);
    DrawSync(0);
    for (level = 0; level < 0x80; level += 8) {
        logo.r0 = level;
        logo.g0 = level;
        logo.b0 = level;
        DrawPrim(&logo);
        VSync(0);
    }
    for (frame = 0x6D; frame != -1; frame--) {
        VSync(0);
    }
    for (level = 0x80; level >= 0; level -= 8) {
        logo.r0 = level;
        logo.g0 = level;
        logo.b0 = level;
        DrawPrim(&logo);
        VSync(0);
    }
    heap_free(image);
}

/* 80019EF8: Fatal error screen: dump the heap log to the PC (or, without one, clear
 * the screen red and hang), then print the error, its caller and heap details every
 * frame forever. */
void mode_show_fatal_error(s32 error, u32 caller) {
    DRAWENV draw[2];
    DISPENV disp[2];
    RECT rect;
    RECT unused; /* unused in the original; reserves 8 bytes */
    s32 frame;
    s32 first;
    s32 second;

    frame = 0;
    if (mode_disc_mode != 0 && mode_disc_mode != -1) {
        heap_write_report_file("c:\\lserrmem.txt");
    } else {
        rect.x = 0;
        rect.y = 0;
        rect.w = 0x280;
        rect.h = 0x1E0;
        ClearImage(&rect, 0xFF, 0, 0);
        for (;;) {
        }
    }
    heap_force_free_all();
    SetDefDrawEnv(&draw[0], 0, 0, 0x180, 0xF0);
    SetDefDispEnv(&disp[0], 0, 0xF0, 0x180, 0xF0);
    SetDefDrawEnv(&draw[1], 0, 0xF0, 0x180, 0xF0);
    SetDefDispEnv(&disp[1], 0, 0, 0x180, 0xF0);
    DrawSyncCallback(0);
    pad_set_vblank_hook(0);
    console_open(0x10, 0x10, 0x120, 0xF0, 0x1F4, 0, 0x3C0, 0x100, 0x3C0, 0x1FF, 0);
    mode_fatal_error_count++;
    draw[1].isbg = 1;
    draw[0].isbg = 1;
    draw[0].r0 = 0;
    draw[0].g0 = 0;
    draw[0].b0 = 0;
    draw[1].r0 = 0;
    draw[1].g0 = 0;
    draw[1].b0 = 0;
    SetDispMask(1);
loop:
    {
        PutDrawEnv((frame & 1) ? &draw[0] : &draw[1]);
        PutDispEnv((frame & 1) ? &disp[0] : &disp[1]);
        console_flush(0);
        console_printf("System Error No %d\n", error);
        console_printf("From  %08x\n", caller);
        console_printf("Count %d\n", mode_fatal_error_count);
        console_printf("Frame %d\n", frame);
        console_printf("MCBlog -> c:\\lserrmem.txt\n");
        console_printf("\n");
        if (error & 0x80) {
            console_printf("%s\n", mode_fatal_error_messages[error - 0x80]);
            if (error == 0x82) {
                heap_get_last_request(&first, &second);
                console_printf("Program From %08x\n", first);
                console_printf("Failure Size %d (%xh) byte \n", second, second);
            }
            if (error == 0x85) {
                heap_get_last_request(&first, &second);
                console_printf("Program From %08x\n", first);
                console_printf("Failure Pointer %p\n", second);
            }
        }
        VSync(0);
        frame++;
    }
    goto loop;
}

s32 mode_kernel_menu_cursor = 0; /* 8004F2D8: kernel menu cursor */

/* Staff names; nothing reads the table. */
char *mode_unreferenced_staff_names[] = { /* 8004F2DC */
    "YOSHII", "HIGUCHI,MIYAGAWA,MASAKI", "KAZUMI", "SUGIMOTO", "HIGUCHI", "MASAKI",
};

/* Game state reset by 8001aadc. */
s32 mode_unread_play_record_word = 0; /* 8004F2F4 */
s32 mode_field_entered_once = 0; /* 8004F2F8 */
s32 mode_music_cached_seq = 0; /* 8004F2FC */
s32 mode_staff_roll_enabled = 0; /* 8004F300 */
s32 mode_worldmap_area_load_count = 0; /* 8004F304 */
s32 mode_music_load_pending = -1; /* 8004F308 */
s32 mode_field_return_pending = 0; /* 8004F30C */
s32 mode_unread_arena_departure_count = 0; /* 8004F310 */
s32 mode_unread_reset_word = 0; /* 8004F314 */
s32 mode_play_clock_frame_count = 0; /* 8004F318 */
s32 mode_party_file_kind = 0; /* 8004F31C */
s32 mode_party_uses_gear_files = 0; /* 8004F320 */
s32 mode_music_selected_track = 0xFF; /* 8004F324 */
s32 mode_play_clock_flags = 0xFF; /* 8004F328 */
s32 mode_effect_bank_not_preloaded = -1; /* 8004F32C */
s32 mode_read_ahead_map = -1; /* 8004F330 */
s32 mode_read_ahead_slot = -1; /* 8004F334 */
s32 mode_music_loaded_track = -1; /* 8004F338 */
s32 mode_music_loaded_wave = -1; /* 8004F33C */
s32 mode_music_start_full_volume = -1; /* 8004F340 */
s32 mode_text_images_preloaded = 0; /* 8004F344 */
s32 mode_music_reuse_seq = 0; /* 8004F348 */
s32 mode_field_map_id = -1; /* 8004F34C */
s32 mode_menu_request_count = 0; /* 8004F350 */
s32 mode_music_wave_streaming = 0; /* 8004F354 */
s32 mode_music_seq_read_pending = 0; /* 8004F358 */
s32 mode_music_seq_active = 0; /* 8004F35C */
s32 mode_music_wave_bank_loaded = 0; /* 8004F360 */
s32 mode_shared_wave_bank_state = 1; /* 8004F364 */
s32 mode_shared_wave_bank_released = 0; /* 8004F368 */
s32 mode_music_started = 0; /* 8004F36C */
s32 mode_field_standalone = 0; /* 8004F370 */
s32 mode_party_files_pending = 0; /* 8004F374 */
s32 mode_debug_hide_compass = 0; /* 8004F378 */
s32 mode_debug_hide_sprites = 0; /* 8004F37C */
s32 mode_debug_hide_layer = 0; /* 8004F380 */
s16 mode_shared_wave_bank_needs_reload = 0; /* 8004F384 */

/* 8001A1E4: Kernel menu buffer: clear to dark blue and set up the white cursor triangle. */
void mode_kernel_menu_init_buffer(s32 index) {
    KernelBuffer *buffer = &mode_kernel_menu_buffers[index];

    buffer->draw.isbg = 1;
    buffer->draw.dtd = 1;
    buffer->draw.r0 = 0;
    buffer->draw.g0 = 0;
    buffer->draw.b0 = 0x20;
    SetPolyF3(&buffer->cursor);
    buffer->cursor.r0 = 0xFF;
    buffer->cursor.g0 = 0xFF;
    buffer->cursor.b0 = 0xFF;
}

/* 8001A250: Kernel menu set-up: debug text window and the two display buffers. */
void mode_kernel_menu_init(void) {
    RECT unused; /* unused in the original; reserves 8 bytes */

    heap_select_owner_tag(6, 0);
    console_open(8, 0x10, 0x170, 0x1E0, 0x3E8, 1, 0x3C0, 0x100, 0x3C0, 0x1FF, 0);
    SetDefDrawEnv(&mode_kernel_menu_buffers[0].draw, 0, 0, 0x140, 0xE0);
    SetDefDrawEnv(&mode_kernel_menu_buffers[1].draw, 0, 0xF0, 0x140, 0xE0);
    SetDefDispEnv(&mode_kernel_menu_buffers[0].disp, 0, 0xF0, 0x140, 0xE0);
    SetDefDispEnv(&mode_kernel_menu_buffers[1].disp, 0, 0, 0x140, 0xE0);
    mode_kernel_menu_init_buffer(0);
    mode_kernel_menu_init_buffer(1);
}

/* 8001A344: Kernel menu frame: move the cursor over the six modes, start the chosen
 * one, print the menu with the play time and place the cursor. */
void mode_kernel_menu_update(void) {
    char clock[24];
    s32 y;
    KernelBuffer *buffer;

    if (pad_port0_repeated & 0x1000) {
        if (--mode_kernel_menu_cursor < 0) {
            mode_kernel_menu_cursor = 5;
        }
    }
    if (pad_port0_repeated & 0x4000) {
        if (++mode_kernel_menu_cursor >= 6) {
            mode_kernel_menu_cursor = 0;
        }
    }
    if (pad_port0_pressed & 0x20) {
        mode_select_next_mode(mode_kernel_menu_cursor + 1);
        mode_kernel_menu_running = 0;
    }
    sprintf(clock, "%02d:%02d:%02d", pad_play_time_hours, pad_play_time_minutes, pad_play_time_seconds);
    console_printf(" XENOGEARS Kernel MENU\n  %s %s MODE\n\n", clock, mode_disc_mode ? "PC HDD" : "CD EMU");
    console_printf("    Field\n    Battle\n    Worldmap\n    Battling\n    Menu\n    Movie\n\n");
    y = mode_kernel_menu_cursor * 8;
    buffer = mode_kernel_menu_current_buffer;
    *(u32 *)&buffer->cursor.x0 = ((y + 0x28) << 16) | 0x20;
    *(u32 *)&buffer->cursor.x1 = ((y + 0x2C) << 16) | 0x27;
    *(u32 *)&buffer->cursor.x2 = ((y + 0x30) << 16) | 0x20;
}

/* 8001A4B4: Mode 0, the kernel menu: run its frame loop until a mode is chosen, then dispatch. */
void mode_run_kernel_menu(void) {
    u_long *ot;

    mode_kernel_menu_init();
    mode_kernel_menu_running = 1;
    mode_kernel_menu_buffer_index = 0;
    do {
        mode_kernel_menu_frame_count++;
        mode_kernel_menu_buffer_index = mode_kernel_menu_frame_count & 1;
        mode_kernel_menu_current_buffer = &mode_kernel_menu_buffers[mode_kernel_menu_buffer_index];
        ot = mode_kernel_menu_current_buffer->ot;
        TermPrim(ot);
        console_flush(ot);
        mode_kernel_menu_update();
        AddPrim(ot, &mode_kernel_menu_current_buffer->cursor);
        DrawSync(0);
        VSync(0);
        PutDrawEnv(&mode_kernel_menu_current_buffer->draw);
        PutDispEnv(&mode_kernel_menu_current_buffer->disp);
        DrawOTag(ot);
    } while (mode_kernel_menu_running != 0 || mode_kernel_menu_buffer_index == 0);
    DrawSync(0);
    mode_dispatch(0);
}

/* 8001A5CC: Allocate the debug screen buffers and clear the two 40x28 cell grids. */
void mode_game_of_life_init(void) {
    s32 row;
    s32 column;

    mode_game_of_life_tiles[0] = heap_alloc(0x3480, 1);
    mode_game_of_life_tiles[1] = heap_alloc(0x3480, 1);
    mode_game_of_life_cells = heap_alloc(0x460, 1);
    mode_game_of_life_neighbor_counts = heap_alloc(0x460, 1);
    for (row = 0; row < 28; row++) {
        for (column = 0; column < 40; column++) {
            mode_game_of_life_cells[row * 40 + column] = 0;
            mode_game_of_life_neighbor_counts[row * 40 + column] = 0;
        }
    }
}

/* 8001A684: Count a hit in a cell of the second grid; out-of-range coordinates wrap
 * to the other edge. */
void mode_game_of_life_count_neighbor(s32 row, s32 column) {
    if (row < 0) {
        row = 28;
    }
    if (row > 28) {
        row = 0;
    }
    if (column < 0) {
        column = 40;
    }
    if (column > 40) {
        column = 0;
    }
    mode_game_of_life_neighbor_counts[row * 40 + column]++;
}

/* 8001A6E8: Debug screen, one Game of Life generation on the 40x28 grid: draw each
 * live cell as an 8x8 tile, count its neighbours, apply the rules and reseed a
 * random walk of 20 cells when fewer than 20 live, plus one random neighbourhood. */
void mode_game_of_life_step(u_long *ot) {
    LifeTile *tile;
    s32 live;
    s32 row; /* also the cell index of the update pass */
    s32 column;

    live = 0;
    tile = mode_game_of_life_tiles[mode_kernel_menu_buffer_index];
    for (row = 0; row < 28; row++) {
        for (column = 0; column < 40; column++) {
            if (mode_game_of_life_cells[row * 40 + column] != 0) {
                setlen(tile, 2);
                tile->rgbc = 0x70280000;
                tile->xy = (column << 3) | (row << 19);
                AddPrim(ot, tile);
                tile++;
                live++;
                mode_game_of_life_count_neighbor(row - 1, column - 1);
                mode_game_of_life_count_neighbor(row - 1, column);
                mode_game_of_life_count_neighbor(row - 1, column + 1);
                mode_game_of_life_count_neighbor(row, column - 1);
                mode_game_of_life_count_neighbor(row, column + 1);
                mode_game_of_life_count_neighbor(row + 1, column - 1);
                mode_game_of_life_count_neighbor(row + 1, column);
                mode_game_of_life_count_neighbor(row + 1, column + 1);
            }
        }
    }
    for (row = 0; row < 28 * 40; row++) {
        if (mode_game_of_life_neighbor_counts[row] != 2) {
            mode_game_of_life_cells[row] = mode_game_of_life_neighbor_counts[row] == 3;
        }
        mode_game_of_life_neighbor_counts[row] = 0;
    }
    if (live < 20) {
        live = 0;
        row = rand() % 28;
        column = rand() % 40;
        do {
            row += rand() % 3 - 1;
            column += rand() % 3 - 1;
            if (row < 0) {
                row = 28;
            }
            if (row > 28) {
                row = 0;
            }
            if (column < 0) {
                column = 40;
            }
            if (column > 40) {
                column = 0;
            }
            live++;
            mode_game_of_life_cells[row * 40 + column] = 1;
        } while (live < 20);
    }
    row = rand() % 28;
    column = rand() % 40;
    mode_game_of_life_count_neighbor(row - 1, column - 1);
    mode_game_of_life_count_neighbor(row - 1, column);
    mode_game_of_life_count_neighbor(row - 1, column + 1);
    mode_game_of_life_count_neighbor(row, column - 1);
    mode_game_of_life_count_neighbor(row, column + 1);
    mode_game_of_life_count_neighbor(row + 1, column - 1);
    mode_game_of_life_count_neighbor(row + 1, column);
    mode_game_of_life_count_neighbor(row + 1, column + 1);
}

/* 8001AADC: Reset the game-wide state words and flags. */
void mode_reset_game_state(void) {
    s32 i;
    s32 *last;

    mode_shared_wave_bank_state = 1;
    mode_play_clock_flags = 0xFF;
    mode_music_selected_track = 0xFF;
    mode_music_cached_seq = 0;
    mode_music_started = 0;
    mode_field_entered_once = 0;
    mode_party_file_kind = 0;
    mode_party_uses_gear_files = 0;
    mode_unread_reset_word = 0;
    mode_unread_arena_departure_count = 0;
    mode_field_return_pending = 0;
    mode_field_standalone = 0;
    mode_music_seq_active = 0;
    mode_music_wave_bank_loaded = 0;
    mode_party_files_pending = 0;
    mode_music_seq_read_pending = 0;
    mode_music_wave_streaming = 0;
    mode_menu_request_count = 0;
    mode_unread_play_record_word = 0;
    mode_text_images_preloaded = 0;
    mode_music_reuse_seq = 0;
    mode_worldmap_area_load_count = 0;
    mode_shared_wave_bank_released = 0;
    mode_staff_roll_enabled = 0;
    mode_debug_hide_layer = 0;
    mode_debug_hide_sprites = 0;
    mode_debug_hide_compass = 0;
    mode_battle_return_fade = 0;
    mode_result_code = 0;
    mode_shared_wave_bank_needs_reload = 0;
    mode_play_clock_frame_count = 0;
    mode_read_ahead_slot = -1;
    mode_field_map_id = -1;
    mode_music_loaded_wave = -1;
    mode_music_loaded_track = -1;
    mode_read_ahead_map = -1;
    mode_effect_bank_not_preloaded = -1;
    mode_music_start_full_volume = -1;
    mode_music_load_pending = -1;
    for (i = 0; i < 3; i++) {
        mode_party_file_ids[i] = 0;
        mode_party_stand_in_actors[i] = 0;
        mode_party_actors[i] = 0;
        mode_party_members[i] = 0;
    }
    for (i = 3, last = &mode_wave_bank_slots[3]; i >= 0; i--) {
        *last-- = 0;
    }
}

/* 8001AC94: Clear the state word 8004f30c (also cleared by a battle defeat). */
void mode_clear_field_return(void) {
    mode_field_return_pending = 0;
}

/* 8001ACA4: Select heap tag 8 and directory 4, then load file 1 of it. */
void mode_preload_field_files(void) {
    mode_read_ahead_slot = -1;
    mode_read_ahead_map = -1;
    heap_select_owner_tag(8, 0);
    cd_select_directory(4, 0);
    mode_reload_party_files(1);
}

/* 8001ACF0: The gear character `index` pilots (0xff none). */
s32 mode_get_character_gear_id(s32 index) {
    return game_current_data->characters[index].gearId;
}

/* 8001AD1C: Wait until the disc is idle, then for the pending read (80028a60). */
void mode_wait_for_disc_idle(void) {
    while (cd_get_pending_read_count() != 0) {
    }
    cd_sync_reads(0);
}

/* 8001AD4C: Take the party from the game data and load each member's field
 * character file (member + 5) into a kept block. */
void mode_load_party_character_files(void) {
    s32 i;
    s32 count;
    u8 member;

    game_current_data = &game_data;
    for (i = 0, count = 0; i < 3; i++) {
        mode_party_members[i] = 0xFF;
        member = game_current_data->party[i];
        if (member != 0xFF) {
            mode_party_members[count++] = member;
        }
    }
    for (i = 0, count = 0; i < 3; i++) {
        if (mode_party_members[i] != 0xFF) {
            mode_party_file_list[count].file = mode_party_members[i] + 5;
            mode_party_file_ids[i] = mode_party_members[i];
            mode_party_file_blocks[count] = heap_alloc(cd_get_aligned_file_size(mode_party_members[i] + 5), 0);
            mode_party_file_list[count].destination = mode_party_file_blocks[count];
            heap_protect_block(mode_party_file_blocks[count]);
            count++;
        }
    }
    mode_party_file_list[count].destination = NULL;
    mode_party_file_list[count].file = 0;
    cd_read_file_list(mode_party_file_list, 0, 0);
    mode_party_file_kind = 1;
}

/* 8001AEB8: As 8001ad4c, but load each member's gear file instead (16 + the gear of
 * the character record, 0xff meaning none). */
void mode_load_party_gear_files(void) {
    s32 i;
    s32 count;
    s32 file;
    u8 member;

    game_current_data = &game_data;
    for (i = 0, count = 0; i < 3; i++) {
        mode_party_members[i] = 0xFF;
        member = game_current_data->party[i];
        if (member != 0xFF) {
            mode_party_members[count++] = member;
        }
    }
    for (i = 0, count = 0; i < 3; i++) {
        if (mode_party_members[i] != 0xFF) {
            file = mode_get_character_gear_id(mode_party_members[i]);
            if (file == 0xFF) {
                file = 0;
            }
            file += 0x10;
            mode_party_file_list[count].file = file + 5;
            mode_party_file_ids[i] = file;
            mode_party_file_blocks[count] = heap_alloc(cd_get_aligned_file_size(file + 5), 0);
            mode_party_file_list[count].destination = mode_party_file_blocks[count];
            heap_protect_block(mode_party_file_blocks[count]);
            count++;
        }
    }
    mode_party_file_list[count].destination = NULL;
    mode_party_file_list[count].file = 0;
    cd_read_file_list(mode_party_file_list, 0, 0);
    mode_party_file_kind = 2;
}

/* 8001B044: Make sure the party files match the current state: characters on foot,
 * gears when 8004f34c has 0xc000 set. */
void mode_sync_party_files(void) {
    mode_wait_for_disc_idle();
    if (mode_party_files_pending != 1) {
        if (mode_field_return_pending != 0) {
            mode_reload_party_files(0);
            return;
        }
    } else {
        mode_unpack_party_files();
        if (mode_field_return_pending != 0) {
            return;
        }
    }
    if ((mode_field_map_id & 0xC000) == 0) {
        mode_party_uses_gear_files = 0;
    } else {
        mode_party_uses_gear_files = 1;
    }
    mode_party_files_pending = 0;
    if (mode_party_uses_gear_files == 0) {
        if (mode_party_file_kind != 1) {
            mode_load_party_character_files();
            mode_party_files_pending = 1;
        }
    } else {
        if (mode_party_file_kind != 2) {
            mode_load_party_gear_files();
            mode_party_files_pending = 1;
        }
    }
}

/* 8001B158: Reload the party files listed in 8006fabc (quietly, from the heap top),
 * plus files 0xa7/0xa8 when asked; on a failed allocation release what was loaded. */
void mode_reload_party_files(s32 extra) {
    s32 i;
    s32 count;

    heap_set_quiet_failures(1);
    if (mode_party_files_pending == 1) {
        mode_unpack_party_files();
    }
    mode_wait_for_disc_idle();
    for (i = 0, count = 0; i < 3; i++) {
        if (mode_party_file_ids[i] != 0xFF) {
            mode_party_file_list[count].file = mode_party_file_ids[i] + 5;
            mode_party_file_blocks[count] = heap_alloc(cd_get_aligned_file_size(mode_party_file_ids[i] + 5), 1);
            mode_party_file_list[count].destination = mode_party_file_blocks[count];
            if (mode_party_file_list[count].destination == NULL) {
                for (i = 0; i < count; i++) {
                    heap_unprotect_block(mode_party_file_blocks[i]);
                    heap_free(mode_party_file_blocks[i]);
                }
                heap_set_quiet_failures(0);
                return;
            }
            heap_protect_block(mode_party_file_blocks[count]);
            count++;
        }
    }
    if (extra) {
        mode_preloaded_text_images = heap_alloc(cd_get_aligned_file_size(0xA7), 1);
        mode_party_file_list[count].destination = mode_preloaded_text_images;
        if (mode_preloaded_text_images != NULL) {
            heap_protect_block(mode_preloaded_text_images);
            mode_party_file_list[count++].file = 0xA7;
            mode_text_images_preloaded = 1;
        }
        mode_preloaded_effect_bank = heap_alloc(cd_get_aligned_file_size(0xA8), 1);
        mode_party_file_list[count].destination = mode_preloaded_effect_bank;
        if (mode_preloaded_effect_bank != NULL) {
            heap_protect_block(mode_preloaded_effect_bank);
            mode_party_file_list[count++].file = 0xA8;
            mode_effect_bank_not_preloaded = 0;
        }
    }
    mode_party_file_list[count].destination = NULL;
    mode_party_file_list[count].file = 0;
    cd_read_file_list(mode_party_file_list, 0, 0);
    mode_party_files_pending = 1;
    heap_set_quiet_failures(0);
}

/* 8001B3A8: Once the party files are read, unpack each member's file into its field
 * sprite block (8005a414) and release it. */
void mode_unpack_party_files(void) {
    s32 i;

    if (mode_party_files_pending != 0) {
        mode_wait_for_disc_idle();
        for (i = 0; i < 3; i++) {
            heap_unprotect_block(mode_party_sprite_blocks[i]);
            if (mode_party_members[i] != 0xFF) {
                heap_unprotect_block(mode_party_file_blocks[i]);
                text_unpack_lzss(mode_party_file_blocks[i], mode_party_sprite_blocks[i]);
                heap_free(mode_party_file_blocks[i]);
            }
        }
        mode_party_files_pending = 0;
    }
}

/* 8001B484: Read a map's data ahead into its own block unless that map is already
 * loaded; returns 0 when loaded, -1 while the disc is busy or after starting the
 * read. */
s32 mode_read_map_ahead(s32 map, s32 slot) {
    if (mode_read_ahead_slot != slot || mode_read_ahead_map != map) {
        if (cd_get_pending_read_count() == 0) {
            cd_sync_reads(0);
            if (mode_read_ahead_slot != -1) {
                heap_unprotect_block(mode_read_ahead_block);
                heap_free(mode_read_ahead_block);
            }
            mode_start_map_read(map);
            mode_read_ahead_slot = slot;
            mode_read_ahead_map = map;
        }
        return -1;
    }
    return 0;
}

/* 8001B53C: Start reading map file 0xb8 + map into a kept block from the heap top. */
void mode_start_map_read(s32 map) {
    s32 file = map + 0xB8;

    mode_read_ahead_size = cd_get_aligned_file_size(file);
    mode_read_ahead_block = heap_alloc(mode_read_ahead_size, 1);
    heap_protect_block(mode_read_ahead_block);
    cd_read_file(file, mode_read_ahead_block, 0, 0x80);
}

/* 8001B5A8: Release the transferred wave bank if it was loaded for this music. */
void mode_release_music_wave_bank(void) {
    if (mode_music_wave_bank_loaded == 1) {
        sound_release_wave_bank(mode_music_wave_bank);
        mode_music_wave_bank_loaded = 0;
    }
}

/* 8001B5E8: Stop the active sequence; release it unless it is kept for reuse, in
 * which case it becomes the cached sequence. */
void mode_stop_music_seq(void) {
    if (mode_music_seq_active == 1) {
        sound_stop_seq((SoundSeq *)mode_music_seq);
        if (mode_music_reuse_seq == 0) {
            sound_release_seq((struct SoundSeq *)mode_music_seq);
        } else {
            mode_music_cached_seq = mode_music_seq;
        }
        mode_music_seq_active = 0;
        mode_music_reuse_seq = 0;
    }
}

/* 8001B66C: Stop the music and forget the loaded sequence and wave bank. */
void mode_stop_music(void) {
    if (mode_music_started != 0) {
        mode_stop_music_seq();
        mode_release_music_wave_bank();
    }
    mode_music_loaded_wave = -1;
    mode_music_loaded_track = -1;
    mode_music_started = 0;
}

/* 8001B6BC: An empty step of the boot (80019578). */
void boot_empty_step(void) {
}
