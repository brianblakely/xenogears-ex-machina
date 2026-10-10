/* Mode 6, the movie mode (overlay image at 0x8006faf0, packed in disc 1 file
 * 40 / disc 2 file 35). The resident mode dispatcher loads it into the mode
 * overlay area and enters 800737ec: that loads the movie library (disc file
 * 19 at 0x801d3000), plays the movie that the request bytes 8004fe44..47
 * name and selects the next mode. Without a request it runs a development
 * menu: movie test settings, a CD-ROM monitor, a CD-ROM read check, a FAT
 * check and a disc change test, drawn with the resident debug font.
 *
 * Everything after the overlay number is this unit: rodata 8006FAF4-800704E8,
 * text 800704E8-80076E48, data 80076E48-80076F3C and the .bss to 80077458.
 * Its rodata starts at 4 mod 8, the phase of all six of its jump tables,
 * which is why the number is a unit of its own (movie_number.c). */
#include "common.h"
#include "movie_mode.h"

/* The unit's .data (0x80076e48..0x80076f3c); the .bss follows. */
s32 D_80076E48 = 0;             /* read check state: the command running */
s32 D_80076E4C = 0;             /* verify errors */
s32 D_80076E50 = 0;             /* never read */
s32 D_80076E54 = 0;             /* never read */
s32 D_80076E58 = 0;             /* never read */
s32 D_80076E5C = 0;             /* vertical blanks counted */
s32 D_80076E60 = 0;             /* never read */
s32 D_80076E64 = 0;             /* read phase: waiting, reading, verifying */
s32 D_80076E68 = 0;             /* last error: offset */
s32 D_80076E6C = 0;             /* size */
s32 D_80076E70 = 0;             /* and the resident's stream counters */
s32 D_80076E74 = 0;
s32 D_80076E78 = 0;
FileEntry *D_80076E7C = NULL;   /* stream list */
FileEntry *D_80076E80 = NULL;   /* verify copy of the stream list */
s32 *D_80076E84 = NULL;         /* stream destination */
s32 *D_80076E88 = NULL;         /* read buffer */
s32 *D_80076E8C = NULL;         /* verify copy of the read */
s32 D_80076E90 = 0;             /* read size */
s32 D_80076E94 = 0;             /* stream bytes left */
StreamRing *D_80076E98 = NULL;  /* stream buffer */
s32 *D_80076E9C = NULL;         /* arrived stream chunk */
void *D_80076EA0 = NULL;        /* FAT check read buffer */
s32 D_80076EA4 = 0;             /* reads ended */
s32 D_80076EA8 = 0;             /* reads in total */
s32 D_80076EAC = -1;            /* class marked " BEFORE" (-1: none) */
s32 D_80076EB0 = -1;            /* class marked " NOW" */
s32 D_80076EB4 = 0;             /* stream list entry */
s32 D_80076EB8 = 0;             /* 1: stream copy; 2: host read */
s32 D_80076EBC = 0;             /* random commands */
s32 D_80076EC0 = 0;             /* never read */
s32 D_80076EC4 = 0;             /* never read */
s32 D_80076EC8 = 0;             /* seconds counted */
s32 D_80076ECC = 0;             /* frames of the current second */
/* Never read: a string, three words and a record of 233, the string and the
 * record's own address. */
char D_80076ED0[] = "\n";
s32 D_80076ED4[3] = {20, 30, 40};
struct {
    s32 value;
    char *text;
    void *self;
} D_80076EE0 = {233, D_80076ED0, &D_80076EE0};
s32 D_80076EEC = 0;             /* menu: frames the buttons were held */
s32 D_80076EF0 = 16;            /* frames until they repeat */
s32 D_80076EF4 = 1234567890;    /* random number state */
s32 D_80076EF8 = 987654321;
s32 D_80076EFC = 0;             /* monitor sector */
s32 D_80076F00 = 0;             /* monitor row */
s32 D_80076F04 = 0;             /* playback: frames the buttons were held */
s32 D_80076F08 = 90;            /* frames until they repeat */
/* Never read: the origin and three 128-long axes, beside the playback
 * camera's translation. */
SVECTOR D_80076F0C[4] = {{0, 0, 0}, {128, 0, 0}, {0, 128, 0}, {0, 0, 128}};
VECTOR D_80076F2C = {0};        /* the playback camera's translation */

/* The unit's .bss (80076f3c-80077458), not in the file: the resident's mode
 * table clears it before entering the overlay (movie.bss.ld). The variables
 * are defined here in address order, the order of their first declaration,
 * in which GCC emits tentative definitions, each in a slot of whole words
 * (decomp/Makefile); the decoded image ends 7 bytes into the first. */
s32 D_80076F3C[16];      /* reads per result class */
u8 *D_80076F7C;          /* host stream: the next frame's data (80028f30) */
StreamFrame *D_80076F80; /* and its first sector header */
u8 D_80076F84[8];        /* CD command result */
/* Menu backdrop: each corner's color fades from one random color to the
 * next over a random number of frames. */
CVECTOR D_80076F8C[4];   /* from */
CVECTOR D_80076F9C[4];   /* to */
s32 D_80076FAC[4];       /* frames into the fade */
s32 D_80076FBC[4];       /* frames of the fade */
CVECTOR D_80076FCC[4];   /* menu frame: from */
CVECTOR D_80076FDC[4];   /* to */
s32 D_80076FEC[4];       /* frames into the fade */
s32 D_80076FFC[4];       /* frames of the fade */
SoundSeq *D_8007700C;    /* battle music sequence (8007548c) */
/* Movie playback. */
s32 D_80077010;          /* last frame the library loaded */
s32 D_80077014;          /* 1: stop; 2..5: frames until then */
s32 D_80077018;          /* buffer the frame went to */
s32 D_8007701C;          /* buffer on display */
s32 D_80077020;          /* decoding paused */
s32 D_80077024;          /* the first buffer's y */
s32 D_80077028;          /* buttons do not end the movie */
/* The playback camera (unused by the movie path). */
VECTOR D_8007702C;       /* eye */
VECTOR D_8007703C;       /* target */
s32 D_8007704C;          /* roll */
MATRIX D_80077050;       /* world to screen */
MATRIX D_80077070;       /* light colors */
MATRIX D_80077090;       /* light directions */
SVECTOR D_800770B0;      /* camera rotation */
MATRIX D_800770B8;       /* camera translation */
MATRIX D_800770D8;       /* camera rotation */
MATRIX D_800770F8;       /* base rotation (identity) */
s32 D_80077118;          /* menu cursor */
s32 D_8007711C;          /* movie index */
MovieBuffer *D_80077120; /* buffer being drawn */
MovieBuffer D_80077124[2];
s32 D_80077394;          /* statistics shown */
s32 D_80077398;          /* XA channel */
s32 D_8007739C;          /* last frame */
s32 D_800773A0;          /* rows */
s32 D_800773A4;          /* first frame */
s32 D_800773A8;          /* start sector */
s32 D_800773AC;          /* buttons */
s32 D_800773B0;          /* monitor shown */
s32 D_800773B4;          /* previous buttons */
s32 D_800773B8[32];      /* VSync(1) before and after each decode step */
s32 D_80077438;          /* split display */
s32 D_8007743C;          /* end frame: 0 changed, 1 found, 2 not found */
s32 D_80077440;          /* menu shown */
s32 D_80077444;          /* start frame: 1 changed, 2 sought */
s32 D_80077448;          /* movie kind */
s32 D_8007744C;          /* buffer index */
s32 D_80077450;          /* disc mode: 0, -1 or host */
s32 D_80077454;          /* library output mode (bit 0: 24-bit) */

/* The menu's CD-ROM monitor: at 640x240, show the read statistics, the
 * resident's error counters and stream state, a dump of the stream buffer
 * and the reads per result class, run the monitor's input every frame and
 * return to the 320-wide menu on Start once no read is running. The unused
 * locals (mode included) reproduce the original's frame; the original also
 * reads the menu cursor before the exit test and stores it back after. */
void func_800704E8(void) {
    u8 unused[8];
    char mode[3] = {0, 2, 2};
    u8 unused2[0x18];
    s32 button;
    s32 i;
    s32 hours;
    s32 cursor;
    MovieBuffer *buffer;
    u_long *ot;

    stream_select_ring(NULL);
    func_80074B58();
    D_80076EA0 = NULL;
    D_80076EB8 = 1;
    for (i = 15; i >= 0; i--) {
        D_80076F3C[i] = 0;
    }
    SetDefDrawEnv(&D_80077124[0].draw, 0, 0, 640, 240);
    SetDefDispEnv(&D_80077124[0].disp, 0, 240, 640, 240);
    SetDefDrawEnv(&D_80077124[1].draw, 0, 240, 640, 240);
    SetDefDispEnv(&D_80077124[1].disp, 0, 0, 640, 240);
    D_80077124[0].draw.isbg = 1;
    D_80077124[1].draw.isbg = 1;
    cd_select_directory(0xC, 3);
    for (;;) {
        if (D_80077120 == &D_80077124[0]) {
            buffer = &D_80077124[1];
        } else {
            buffer = &D_80077124[0];
        }
        ot = buffer->ot;
        D_80077120 = buffer;
        D_8007744C = 1 - D_8007744C;
        ClearOTagR(ot, 32);
        func_800712C4();
        func_800747AC(0, 0, &button);
        func_80070DCC();
        if (D_80076EBC != 0) {
            console_printf("Random Mode\n");
        }
        if (D_80076EB8 == 0) {
            console_printf("Stream Pause\n");
        }
        console_printf("Read %3d Error %3d VSync %8d EC %2d ST %2d ", D_80076E48, D_80076E4C,
                      D_80076E5C, cd_error_count, cd_command_state);
        switch (D_80076E64) {
        case 0:
            console_printf("Waiting\n");
            break;
        case 1:
            console_printf("Reading\n");
            break;
        case 2:
            console_printf("Verifing\n");
            break;
        }
        console_printf("C1 %3d C2 %3d C3 %3d C4 %3d C5 %3d C6 %3d C7 %3d C8 %3d C9 %3d\n",
                      cd_stat_setloc_count, cd_stat_command_ok_count, cd_stat_command_fail_count, cd_stat_retry_setloc_count, cd_stat_retry_fail_count, cd_stat_lesmem_count,
                      cd_stat_error_limit_count, cd_stat_stop_ok_count, cd_stat_stop_fail_count);
        console_printf("RestFile %7d RestSize %7d N1 %4d N2 %4d N3 %4d R%3d D%3d\n",
                      cd_get_pending_read_count(), cd_get_read_bytes_left(), cd_file_out_of_order_count, cd_list_out_of_order_count, cd_stream_out_of_order_count,
                      stream_next_store_sequence, stream_next_complete_sequence);
        console_printf("ErrorAddress %8x ErrorSize %8x N%3d N%3d N%3d\n", D_80076E68, D_80076E6C,
                      D_80076E70, D_80076E74, D_80076E78);
        console_printf("FrdPtr1 %8x FrdPtr2 %8x Buf1 %8x Buf2 %8x\n", D_80076E7C, D_80076E80,
                      D_80076E88, D_80076E8C);
        if (D_80076E48 >= 11 || D_80076E94 > 0) {
            console_printf("S %8x Adrs %8x Write %8x Rest %8x\n", D_80076E98, D_80076E9C,
                          D_80076E84, D_80076E94);
            for (i = 0; i < 7; i++) {
                console_printf("%08x ", ((s32 *)D_80076E98)[i]);
            }
            console_printf((char *)D_8006FC6C);
            for (i = 0; i < 7; i++) {
                console_printf("%08x ", ((s32 *)D_80076E98)[i + 7]);
            }
            console_printf((char *)D_8006FC6C);
            for (i = 0; i < 7; i++) {
                console_printf("%08x ", ((s32 *)D_80076E98)[i + 14]);
            }
            console_printf((char *)D_8006FC6C);
            for (i = 0; i < 7; i++) {
                console_printf("%08x ", ((s32 *)D_80076E98)[i + 21]);
            }
            console_printf((char *)D_8006FC6C);
            if (D_80076E7C != NULL) {
                console_printf((char *)D_8006FC70, D_80076E7C[0].data, D_80076E7C[1].data,
                              D_80076E7C[2].data, D_80076E7C[3].data);
            }
        }
        D_80076EA8 = 0;
        console_printf((char *)D_8006FC8C, D_80076EA4);
        for (i = 0; i < 13; i++) {
            console_printf((char *)D_8006FC98, i, D_80076F3C[i]);
            D_80076EA8 += D_80076F3C[i];
            if (D_80076EB0 == i) {
                console_printf((char *)D_8006FCA4);
            }
            if (D_80076EAC == i) {
                console_printf((char *)D_8006FCAC);
            }
            console_printf((char *)D_8006FC6C);
        }
        hours = D_80076EC8 / 3600;
        console_printf((char *)D_8006FCB4, D_80076EA8, hours, D_80076EC8 / 60 - hours * 60,
                      D_80076EC8 % 60);
        console_printf((char *)D_8006FCD8);
        if (D_80077394 > 0) {
            heap_print_report(1, 0, 6, 0x808D);
        }
        console_flush(D_80077120->ot);
        func_80072F98(D_80077120->ot, (POLY_G4 *)D_80077120->box, 8, 12, 624, 216);
        func_800734B8(D_80077120->ot, (POLY_G4 *)D_80077120->frame, 7, 11, 626, 218);
        DrawSync(0);
        VSync(0);
        PutDrawEnv(&D_80077120->draw);
        PutDispEnv(&D_80077120->disp);
        DrawOTag(&D_80077120->ot[31]);
        cursor = D_80077118;
        if (!(D_800773B4 & 0x800) && (D_800773AC & 0x800) && D_80076E48 == 0) {
            D_80077394 = 0;
            cd_stop_read(0);
            cd_sync_reads(0);
            if (D_80076EA0 != NULL) {
                heap_free(D_80076EA0);
            }
            D_80076EA0 = NULL;
            if (D_80076E98 != NULL) {
                heap_free(D_80076E98);
            }
            D_80076E98 = NULL;
            func_80074B58();
            SetDefDrawEnv(&D_80077124[0].draw, 0, 0, 320, 240);
            SetDefDispEnv(&D_80077124[0].disp, 0, 240, 320, 240);
            SetDefDrawEnv(&D_80077124[1].draw, 0, 240, 320, 240);
            SetDefDispEnv(&D_80077124[1].disp, 0, 0, 320, 240);
            D_80077124[0].disp.screen.x = 0;
            D_80077124[0].disp.screen.y = 10;
            D_80077124[0].disp.screen.w = 256;
            D_80077124[0].disp.screen.h = 216;
            D_80077124[1].disp.screen.x = 0;
            D_80077124[1].disp.screen.y = 10;
            D_80077124[1].disp.screen.w = 256;
            D_80077124[1].disp.screen.h = 216;
            D_80077124[0].draw.isbg = 1;
            D_80077124[1].draw.isbg = 1;
            return;
        }
        D_80077118 = cursor;
    }
}

/* CD-ROM monitor input: newly pressed buttons issue the monitor's CD
 * commands or read steps; R1 toggles the stream copy (1) or host read (2),
 * Select random commands. A running stream copies each arrived chunk
 * into the destination and moves to the next list entry when it ends. */
void func_80070DCC(void) {
    s32 i;
    s32 command;

    if (!(D_800773B4 & 0x20) && (D_800773AC & 0x20)) {
        func_80071C34(1);
    }
    if (!(D_800773B4 & 0x10) && (D_800773AC & 0x10)) {
        func_80071C34(2);
    }
    if (!(D_800773B4 & 0x80) && (D_800773AC & 0x80)) {
        func_80071C34(3);
    }
    if (!(D_800773B4 & 0x40) && (D_800773AC & 0x40)) {
        D_80076EA4++;
        cd_stop_read(0);
    }
    if (!(D_800773B4 & 0x1000) && (D_800773AC & 0x1000)) {
        func_80071C34(7);
    }
    if (!(D_800773B4 & 0x4000) && (D_800773AC & 0x4000)) {
        func_80071C34(9);
    }
    if (!(D_800773B4 & 8) && (D_800773AC & 8)) {
        D_80076EB8 = 1 - D_80076EB8;
    }
    if (!(D_800773B4 & 4) && (D_800773AC & 4) && D_80076E48 < 11) {
        func_80071BA0();
    }
    if (D_80076EB8 == 1) {
        D_80076E9C = (s32 *)stream_get_next_chunk();
        if (D_80076E9C != NULL) {
            i = 0;
            if (D_80076E94 > 0x800) {
                do {
                    *D_80076E84++ = D_80076E9C[i++];
                } while (i < 0x200);
            } else {
                while (i < D_80076E94 / 4) {
                    *D_80076E84++ = D_80076E9C[i++];
                }
            }
            D_80076E94 -= 0x800;
            if (D_80076E94 <= 0 && D_80076E48 == 12) {
                i = D_80076E7C[++D_80076EB4].id;
                if (i != 0) {
                    D_80076E94 = cd_get_aligned_file_size(i);
                }
                D_80076E84 = D_80076E7C[D_80076EB4].data;
            }
            stream_release_chunk((u8 *)D_80076E9C);
        }
    }
    if (D_80076EB8 == 2 && stream_get_next_movie_frame(&D_80076F7C, &D_80076F80) == 0) {
        stream_release_movie_frame((u8 *)D_80076F80);
    }
    if (!(D_800773B4 & 0x2000) && (D_800773AC & 0x2000)) {
        func_80071C34(11);
    }
    if (!(D_800773B4 & 0x8000) && (D_800773AC & 0x8000)) {
        func_80071C34(13);
    }
    if (D_80076EBC != 0) {
        command = func_80074AF0() & 0xFF;
        if (command == 0 && D_80076E48 < 11) {
            func_80071BA0();
        }
        if ((u32)(command - 1) < 12) {
            func_80071C34(command);
        }
    }
    if (!(D_800773B4 & 0x100) && (D_800773AC & 0x100)) {
        D_80076EBC = 1 - D_80076EBC;
    }
}

/* func_800704E8's other strings, after its literals. The unit's literal "\n"
 * is func_800737EC's, so this "\n" is an array, and the strings after it
 * are too. */
const char D_8006FC6C[] = "\n";
const char D_8006FC70[] = "%08x %08x %08x %08x %08x\n";
const char D_8006FC8C[] = "Cancel%8d\n";
const char D_8006FC98[] = "CT%1x   %8d";
const char D_8006FCA4[] = " NOW";
const char D_8006FCAC[] = " BEFORE";
const char D_8006FCB4[] = "\nTOTAL %8d : Time %3d:%02d:%02d\n";
const char D_8006FCD8[] = "\nPUSH START BUTTON TO MENU.";

/* Fill `size` bytes of `words` with `value`, counting in `n`. */
#define FILL_WORDS(words, size, n, value)  \
    for (n = 0; n < (size) / 4; n++) {    \
        (words)[n] = (value);              \
    }

/* Record a verify mismatch at byte `offset` of a `size`-byte read: the
 * first one keeps its place and the resident's stream counters. */
#define VERIFY_ERROR(offset, size)         \
    {                                      \
        if (D_80076E4C == 0) {             \
            D_80076E68 = (offset);         \
            D_80076E6C = (size);           \
            D_80076E70 = cd_file_out_of_order_count;       \
            D_80076E74 = cd_list_out_of_order_count;       \
            D_80076E78 = cd_stream_out_of_order_count;       \
        }                                  \
        D_80076E4C++;                      \
    }

/* Monitor read check, run every frame: once the command's read (phase 1)
 * ends, read the same data again into a second buffer or file list filled
 * with -1 (phase 2); once that ends, compare both copies word by word,
 * record the first mismatch and release the copies. */
void func_800712C4(void) {
    s32 i;
    s32 *dest;
    s32 index;
    s32 file;
    s32 *buffer;
    s32 *copy;

    if (D_80076E64 == 0) {
        return;
    }
    if (cd_get_pending_read_count() == 0 && D_80076E64 == 1) {
        D_80076E64 = 2;
        switch (D_80076E48) {
        case 1:
            D_80076E90 = 0x2000;
            D_80076E8C = buffer = heap_alloc(0x2000, 0);
            FILL_WORDS(buffer, D_80076E90, i, -1);
            cd_read_raw_sectors(0x40, D_80076E8C, D_80076E90, 0, 0);
            break;
        case 2:
            D_80076E90 = cd_get_aligned_file_size(7);
            D_80076E8C = buffer = heap_alloc(D_80076E90, 0);
            FILL_WORDS(buffer, D_80076E90, i, -1);
            cd_read_file(7, D_80076E8C, 0, 0);
            break;
        case 3:
        case 12:
            D_80076E80 = cd_alloc_directory_file_table(2, 0);
            if (D_80076E80 == NULL) {
                D_80076E64 = 0;
                D_80076E48 = 0;
                break;
            }
            for (index = 0; (file = D_80076E80[index].id) > 0; index++) {
                copy = D_80076E80[index].data;
                D_80076E90 = cd_get_aligned_file_size(file);
                FILL_WORDS(copy, D_80076E90, i, -1);
            }
            cd_read_file_list((FileRequest *)D_80076E80, 0, 0);
            break;
        case 4:
            D_80076E90 = 0x2000;
            D_80076E8C = buffer = heap_alloc(0x2000, 0);
            FILL_WORDS(buffer, D_80076E90, i, -1);
            cd_read_raw_sectors(0x40, D_80076E8C, D_80076E90, 0, 0);
            break;
        case 5:
            D_80076E90 = cd_get_aligned_file_size(7);
            D_80076E8C = buffer = heap_alloc(D_80076E90, 0);
            FILL_WORDS(buffer, D_80076E90, i, -1);
            cd_read_file(7, D_80076E8C, 1, 0);
            break;
        case 6:
            D_80076E80 = cd_alloc_directory_file_table(2, 0);
            if (D_80076E80 == NULL) {
                D_80076E64 = 0;
                D_80076E48 = 0;
                break;
            }
            for (index = 0; (file = D_80076E80[index].id) > 0; index++) {
                copy = D_80076E80[index].data;
                D_80076E90 = cd_get_aligned_file_size(file);
                FILL_WORDS(copy, D_80076E90, i, -1);
            }
            cd_read_file_list((FileRequest *)D_80076E80, 1, 0);
            break;
        case 7:
        case 8:
            D_80076E64 = 0;
            D_80076E48 = 0;
            break;
        case 11:
            D_80076E90 = cd_get_aligned_file_size(6);
            D_80076E8C = buffer = heap_alloc(D_80076E90, 0);
            FILL_WORDS(buffer, D_80076E90, i, -1);
            cd_read_file(6, D_80076E8C, 0, 0);
            break;
        }
    }
    if (cd_get_pending_read_count() == 0 && D_80076E64 == 2) {
        D_80076E64 = 0;
        switch (D_80076E48) {
        case 1:
        case 4:
            for (i = 0; i < 0x800; i++) {
                if (D_80076E88[i] != D_80076E8C[i]) {
                    VERIFY_ERROR(i * 4, 0x2000);
                    break;
                }
            }
            heap_free(D_80076E88);
            heap_free(D_80076E8C);
            D_80076E64 = 0;
            break;
        case 2:
        case 5:
            D_80076E90 = cd_get_aligned_file_size(7);
            for (i = 0; i < D_80076E90 / 4; i++) {
                if (D_80076E88[i] != D_80076E8C[i]) {
                    VERIFY_ERROR(i * 4, cd_get_aligned_file_size(7));
                    break;
                }
            }
            heap_free(D_80076E88);
            heap_free(D_80076E8C);
            D_80076E64 = 0;
            break;
        case 3:
        case 6:
        case 12:
            index = 0;
            file = D_80076E7C[0].id;
            if (file > 0) {
                do {
                    copy = D_80076E80[index].data;
                    dest = D_80076E7C[index].data;
                    D_80076E90 = cd_get_aligned_file_size(file);
                    for (i = 0; i < D_80076E90 / 4; i++) {
                        if (dest[i] != copy[i]) {
                            VERIFY_ERROR(i * 4, cd_get_aligned_file_size(file));
                            break;
                        }
                    }
                } while ((file = D_80076E7C[++index].id) > 0);
            }
            cd_free_file_table(D_80076E7C);
            cd_free_file_table(D_80076E80);
            heap_free(D_80076E7C);
            heap_free(D_80076E80);
            D_80076E64 = 0;
            break;
        case 7:
        case 8:
        case 9:
        case 10:
            D_80076E64 = 0;
            break;
        case 11:
            if (D_80076E94 > 0) {
                D_80076E64 = 2;
            } else {
                D_80076E90 = cd_get_aligned_file_size(6);
                for (i = 0; i < D_80076E90 / 4; i++) {
                    if (D_80076E88[i] != D_80076E8C[i]) {
                        VERIFY_ERROR(i * 4, cd_get_aligned_file_size(6));
                        break;
                    }
                }
                heap_free(D_80076E88);
                heap_free(D_80076E8C);
            }
            D_80076E64 = 0;
            break;
        }
        D_80076E48 = 0;
    }
}

/* FAT check step: read file 40h into the check buffer (allocated once) and
 * count the read; a pass without errors keeps its tally. */
void func_80071BA0(void) {
    s32 tally;

    if (D_80076EA0 == NULL) {
        D_80076EA0 = heap_alloc(0x2000, 0);
    }
    if (D_80076EA0 != NULL) {
        cd_read_raw_sectors(0x40, D_80076EA0, 0x2000, 0, 0);
    }
    D_80076F3C[0]++;
    if (D_80076E4C == 0) {
        tally = D_80076EB0;
        D_80076EB0 = 0;
        D_80076EAC = tally;
    }
}

/* Clear `size` bytes of words at `dest`, counting in `n`. */
#define ZERO_WORDS(dest, size, n)          \
    {                                      \
        s32 *word;                         \
        n = 0;                             \
        word = (dest);                     \
        for (; n < (size) / 4; n++) {      \
            *word++ = 0;                   \
        }                                  \
    }

/* Start monitor command `command` when no read is running: 1/4 read file
 * 40h into a fresh 8 KB buffer, 2/5 file 7 through the host-file stream,
 * 3/6 the directory's file list, 7/8 the stream ring, 11 stream file 6 and
 * 12 the file list into the ring, 13 file 3 into a new 64-block ring. Each
 * destination is cleared first; the command counts in its class. */
void func_80071C34(s32 command) {
    s32 i;
    s32 index;
    s32 file;
    s32 *dest;

    if (D_80076E48 != 0 || D_80076E64 != 0) {
        return;
    }
    cd_select_directory(0xC, 3);
    D_80076E64 = 1;
    D_80076E48 = command;
    switch (command) {
    case 1:
        D_80076E90 = 0x2000;
        D_80076F3C[1]++;
        D_80076E88 = heap_alloc(0x2000, 0);
        ZERO_WORDS(D_80076E88, D_80076E90, i);
        cd_read_raw_sectors(0x40, D_80076E88, D_80076E90, 0, 0);
        break;
    case 2:
        D_80076F3C[2]++;
        D_80076E90 = cd_get_aligned_file_size(7);
        D_80076E88 = heap_alloc(D_80076E90, 0);
        ZERO_WORDS(D_80076E88, D_80076E90, i);
        cd_read_file(7, D_80076E88, 0, 0);
        break;
    case 3:
        D_80076F3C[3]++;
        D_80076E7C = cd_alloc_directory_file_table(2, 0);
        if (D_80076E7C == NULL) {
            D_80076E64 = 0;
            D_80076E48 = 0;
            break;
        }
        for (index = 0; (file = D_80076E7C[index].id) > 0; index++) {
            dest = D_80076E7C[index].data;
            D_80076E90 = cd_get_aligned_file_size(file);
            ZERO_WORDS(dest, D_80076E90, i);
        }
        cd_read_file_list((FileRequest *)D_80076E7C, 0, 0);
        break;
    case 4:
        D_80076E90 = 0x2000;
        D_80076F3C[4]++;
        D_80076E88 = heap_alloc(0x2000, 0);
        ZERO_WORDS(D_80076E88, D_80076E90, i);
        cd_read_raw_sectors(0x40, D_80076E88, D_80076E90, 0, 0);
        break;
    case 5:
        D_80076F3C[5]++;
        D_80076E90 = cd_get_aligned_file_size(7);
        D_80076E88 = heap_alloc(D_80076E90, 0);
        ZERO_WORDS(D_80076E88, D_80076E90, i);
        cd_read_file(7, D_80076E88, 1, 0);
        break;
    case 6:
        D_80076F3C[6]++;
        D_80076E7C = cd_alloc_directory_file_table(2, 0);
        if (D_80076E7C == NULL) {
            D_80076E64 = 0;
            D_80076E48 = 0;
            break;
        }
        for (index = 0; (file = D_80076E7C[index].id) > 0; index++) {
            dest = D_80076E7C[index].data;
            D_80076E90 = cd_get_aligned_file_size(file);
            ZERO_WORDS(dest, D_80076E90, i);
        }
        cd_read_file_list((FileRequest *)D_80076E7C, 1, 0);
        break;
    case 7:
        D_80076F3C[7]++;
        if (D_80076E98 == NULL) {
            D_80076E98 = stream_create_ring(4, 0);
        }
        stream_start_image_load(1, D_80076E98, 0, 0, 1, 0, 0, 0, 0, 0);
        break;
    case 8:
        D_80076F3C[8]++;
        if (D_80076E98 == NULL) {
            D_80076E98 = stream_create_ring(4, 0);
        }
        stream_start_image_load(1, D_80076E98, 1, 0, 1, 0, 0, 0, 0, 0);
        break;
    case 11:
        if (D_80076EB8 == 2) {
            D_80076EB8 = 1;
        }
        D_80076F3C[11]++;
        if (D_80076E98 == NULL) {
            D_80076E98 = stream_create_ring(4, 0);
        }
        D_80076E94 = D_80076E90 = cd_get_aligned_file_size(6);
        D_80076E84 = D_80076E88 = heap_alloc(D_80076E90, 0);
        ZERO_WORDS(D_80076E88, D_80076E90, i);
        cd_read_file(6, D_80076E98, 1, 0x100);
        break;
    case 12:
        if (D_80076EB8 == 2) {
            D_80076EB8 = 1;
        }
        D_80076F3C[12]++;
        if (D_80076E98 == NULL) {
            D_80076E98 = stream_create_ring(4, 0);
        }
        D_80076E7C = cd_alloc_directory_file_table(2, 0);
        if (D_80076E7C == NULL) {
            D_80076E64 = 0;
            D_80076E48 = 0;
            break;
        }
        file = D_80076E7C[0].id;
        D_80076E94 = cd_get_aligned_file_size(file);
        D_80076EB4 = 0;
        D_80076E84 = D_80076E7C[0].data;
        for (index = 0; file > 0; file = D_80076E7C[++index].id) {
            dest = D_80076E7C[index].data;
            D_80076E90 = cd_get_aligned_file_size(file);
            ZERO_WORDS(dest, D_80076E90, i);
        }
        stream_select_ring(D_80076E98);
        cd_read_file_list((FileRequest *)D_80076E7C, 1, 0x100);
        break;
    case 13:
        D_80076EB8 = 2;
        D_80076F3C[13]++;
        if (D_80076E98 != NULL) {
            heap_free(D_80076E98);
        }
        D_80076E98 = stream_create_ring(0x40, 0);
        cd_select_directory(0x18, 0);
        cd_read_file(3, D_80076E98, 0, 0x200);
        break;
    }
    if (D_80076E4C == 0) {
        D_80076EAC = D_80076EB0;
        D_80076EB0 = D_80076E48;
    }
}

/* Vertical-blank tick: count frames and whole seconds. */
void func_80072428(void) {
    D_80076E5C++;
    if (++D_80076ECC >= 60) {
        D_80076ECC = 0;
        D_80076EC8++;
    }
}

/* The menu's disc change test: show the test state (the steps reached, the
 * error and the last CD command result); Start begins a test from a stopped
 * or failed state, Cross steps a waiting one, and each frame runs one step
 * of it for the other disc. Circle returns to the menu. */
void func_80072480(void) {
    s32 button;
    s32 error;
    s32 done;
    s32 step;
    s32 state;
    s32 frames;
    MovieBuffer *buffer;
    u_long *ot;

    frames = 0;
    state = 0;
    error = 0;
    done = 1;
    for (;;) {
        if (D_80077120 == &D_80077124[0]) {
            buffer = &D_80077124[1];
        } else {
            buffer = &D_80077124[0];
        }
        ot = buffer->ot;
        D_80077120 = buffer;
        D_8007744C = 1 - D_8007744C;
        ClearOTagR(ot, 32);
        console_printf("\n[ DISC CHANGE TEST NOW DISC %2d ]\n\n", cd_get_disc_number());
        func_800747AC(0, 0, &button);
        console_printf("  STATUS ");
        if (error == 1) {
            console_printf("[ IT IS NOT PLAY STATION DISC ]\n");
        } else if (error == 2) {
            console_printf("[ NOT XENOGEARS DISC ]\n");
        } else if (error == 3) {
            console_printf("[ NO CHANGE DISC ]\n");
        } else if (error == 4) {
            console_printf("[ RETRY SET DISC ]\n");
        } else {
            console_printf("[ NOP ]\n");
        }
        console_printf((char *)D_8006FC6C);
        for (step = 0; step < 9; step++) {
            if (step < state) {
                switch (step) {
                case 0:
                    console_printf("  1 : NORMAL SPEED\n");
                    break;
                case 1:
                    console_printf("  2 : CD STOPED\n");
                    break;
                case 2:
                    console_printf("  3 : CD OPENED\n");
                    break;
                case 3:
                    console_printf("  4 : CD CLOSED\n");
                    break;
                case 4:
                    console_printf("  5 : SPINDLE OK\n");
                    break;
                case 5:
                    console_printf("  6 : TOC OK\n");
                    break;
                case 6:
                    console_printf("  7 : SET LOCATION OK\n");
                    break;
                case 7:
                    console_printf("  8 : PLAY STATION DISC OK\n");
                    break;
                case 8:
                    console_printf("  9 : XENOGEARS %2d DISC OK\n", cd_get_disc_number());
                    break;
                }
            } else {
                console_printf(" %2d :\n", step + 1);
            }
        }
        if (done) {
            console_printf("\n MODE %1d : NO ERROR  COUNT %6d\n", state, frames);
        } else {
            console_printf("\n MODE %1d : %2d ERROR COUNT %6d\n", state, done, frames);
        }
        console_printf(" RESULT %02x %02x %02x %02x %02x %02x %02x %02x\n", D_80076F84[0],
                      D_80076F84[1], D_80076F84[2], D_80076F84[3], D_80076F84[4], D_80076F84[5],
                      D_80076F84[6], D_80076F84[7]);
        console_printf("\n\n PUSH START TO TEST.\n");
        console_printf(" PUSH CIRCLE BUTTON TO MENU.\n");
        if (state > 0) {
            state = func_80072A08(3 - cd_get_disc_number(), state, &error, &done);
        }
        if ((D_800773AC & 0x40) && !(D_800773B4 & 0x40) && state > 0 && state < 8) {
            state++;
        }
        if ((D_800773AC & 0x800) && !(D_800773B4 & 0x800) &&
            (state == 0 || state == 9 || error != 0)) {
            error = 0;
            state = 2;
            func_8007293C();
        }
        if (D_80077394 > 0) {
            D_80077394 = 0;
        }
        console_flush(D_80077120->ot);
        func_80072F98(D_80077120->ot, (POLY_G4 *)D_80077120->box, 8, 20, 304, 192);
        func_800734B8(D_80077120->ot, (POLY_G4 *)D_80077120->frame, 7, 19, 306, 194);
        DrawSync(0);
        VSync(0);
        PutDrawEnv(&D_80077120->draw);
        PutDispEnv(&D_80077120->disp);
        DrawOTag(&D_80077120->ot[31]);
        if (button == 2) {
            break;
        }
        frames++;
    }
}

/* Stop the resident disc read and wait until the drive reports its status. */
void func_8007293C(void) {
    if (cd_has_pc_file_server() == 0) {
        cd_stop_read(0);
        cd_sync_reads(0);
        cd_set_mode(0);
        cd_sync_reads(0);
        VSync(3);
        while (CdControlB(8, NULL, D_80076F84) == 0) {
        }
    }
}

/* Read `size` bytes of the host file `name` into `buffer`. */
void func_800729A8(char *name, void *buffer, s32 size) {
    s32 fd;

    fd = PCopen(name, 0, 0);
    PCread(fd, buffer, size);
    PCclose(fd);
}

/* One step of the disc change test for disc `disc`: stop the drive, wait for
 * the lid to open and close and the spindle, read the TOC, seek sector 0 and
 * check the disc label, then reload the directory tables. With the host PC
 * the tables are read from its files instead. `*error` gets 1 (seek error),
 * 2 (not a Xenogears disc) or 3 (wrong disc); `*done` the command result.
 * Returns the next state. */
s32 func_80072A08(s32 disc, s32 state, s32 *error, s32 *done) {
    u32 label[4] = {0, 0, 0, 0};
    CdlLOC loc;
    s32 result;

    CdIntToPos(0, &loc);
    result = 1;
    if (*error == 0) {
        if (cd_has_pc_file_server() != 0 && state < 9) {
            if (disc == 1) {
                func_800729A8("c:\\work\\cdrom.mdg", cd_file_index, 0x8000);
                func_800729A8("c:\\work\\cdrom.fid", cd_directory_table, 0x7A);
                func_800729A8("c:\\work\\cdrom.fnd", cd_pc_file_names, 0x40000);
            } else {
                func_800729A8("c:\\work\\cdrom2.mdg", cd_file_index, 0x8000);
                func_800729A8("c:\\work\\cdrom2.fid", cd_directory_table, 0x7A);
                func_800729A8("c:\\work\\cdrom2.fnd", cd_pc_file_names, 0x40000);
            }
            state = 9;
        } else {
            switch (state) {
            case 1:
                result = CdControlB(CdlStop, NULL, D_80076F84);
                if (result != 0) {
                    state++;
                }
                break;
            case 2:
                CdControlB(CdlNop, NULL, D_80076F84);
                if (D_80076F84[0] & 0x10) {
                    state++;
                }
                break;
            case 3:
                CdControlB(CdlNop, NULL, D_80076F84);
                if (!(D_80076F84[0] & 0x10)) {
                    state++;
                }
                break;
            case 4:
                result = CdControlB(CdlNop, NULL, D_80076F84);
                if (D_80076F84[0] & 2) {
                    if (result != 0) {
                        state++;
                    }
                }
                break;
            case 5:
                result = CdControlB(CdlGetTN, NULL, D_80076F84);
                if (result != 0) {
                    state++;
                }
                break;
            case 6:
                result = CdControlB(CdlSetloc, (u8 *)&loc, D_80076F84);
                if (result != 0) {
                    state++;
                }
                break;
            case 7:
                result = CdControlB(CdlSeekL, NULL, D_80076F84);
                if ((D_80076F84[0] & 1) && (D_80076F84[1] & 0x40) && result == 0) {
                    *error = 1;
                } else if (result != 0) {
                    state++;
                }
                if (result == 0) {
                    state = 5;
                }
                break;
            case 8:
                cd_set_mode(0xA0);
                cd_sync_reads(0);
                VSync(3);
                cd_read_raw_sectors(0x17, label, 0x10, 0, 0);
                cd_sync_reads(0);
                if (label[1] == 0x4E45585F) { /* "_XEN" */
                    if (((u8 *)label)[3] == disc + '0') {
                        cd_read_raw_sectors(0x18, cd_file_index, 0x8000, 0, 0);
                        state++;
                        cd_sync_reads(0);
                        cd_read_raw_sectors(0x28, cd_directory_table, 0x7A, 0, 0);
                        cd_sync_reads(0);
                        *error = 0;
                    } else {
                        *error = 3;
                    }
                } else {
                    *error = 2;
                }
                break;
            }
        }
        *done = result;
    }
    return state;
}

/* Set up the menu backdrop quads of both buffers at (x, y), w by h, with
 * random dark blue corner fades. */
void func_80072D84(POLY_G4 *poly0, POLY_G4 *poly1, s32 x, s32 y, s32 w, s32 h) {
    s32 i;

    for (i = 0; i < 4; i++) {
        D_80076F8C[i].r = (func_80074AF0() & 0xFF) / 32 + 8;
        D_80076F8C[i].g = 8;
        D_80076F8C[i].b = (func_80074AF0() & 0xFF) / 3 + 16;
        D_80076F9C[i].r = (func_80074AF0() & 0xFF) / 32 + 8;
        D_80076F9C[i].g = 8;
        D_80076F9C[i].b = (func_80074AF0() & 0xFF) / 3 + 16;
    }
    for (i = 0; i < 4; i++) {
        D_80076FAC[i] = 0;
        D_80076FBC[i] = (func_80074AF0() & 0xFF) + 32;
    }
    SetPolyG4(poly0);
    SetSemiTrans(poly0, 0);
    poly0->x0 = x;
    poly0->y0 = y;
    poly0->x1 = x + w;
    poly0->y1 = y;
    poly0->x2 = x;
    poly0->y2 = y + h;
    poly0->x3 = x + w;
    poly0->y3 = y + h;
    SetPolyG4(poly1);
    SetSemiTrans(poly1, 0);
    poly1->x0 = x;
    poly1->y0 = y;
    poly1->x1 = x + w;
    poly1->y1 = y;
    poly1->x2 = x;
    poly1->y2 = y + h;
    poly1->x3 = x + w;
    poly1->y3 = y + h;
}

/* Add the menu backdrop to `ot`: a gouraud quad at (x, y), w by h, whose
 * corner colors each fade toward a new random color (as func_800734B8).
 * Each channel's difference of two bytes is formed in the channel's int and
 * kept in a short for the fade step. */
void func_80072F98(u_long *ot, POLY_G4 *poly, s32 x, s32 y, s32 w, s32 h) {
    s32 i;
    u8 from;
    s16 delta;
    s32 r;
    s32 g;
    s32 b;

    poly->x0 = x;
    poly->y0 = y;
    poly->y1 = y;
    poly->x2 = x;
    poly->x1 = x + w;
    poly->y2 = y + h;
    poly->x3 = x + w;
    poly->y3 = y + h;
    for (i = 0; i < 4; i++) {
        if (++D_80076FAC[i] > D_80076FBC[i]) {
            D_80076FAC[i] = 0;
            D_80076FBC[i] = (func_80074AF0() & 0xFF) + 32;
            D_80076F8C[i].r = D_80076F9C[i].r;
            D_80076F8C[i].g = D_80076F9C[i].g;
            D_80076F8C[i].b = D_80076F9C[i].b;
            D_80076F9C[i].r = (func_80074AF0() & 0xFF) / 32 + 8;
            D_80076F9C[i].g = 8;
            D_80076F9C[i].b = (func_80074AF0() & 0xFF) / 3 + 16;
        }
        from = D_80076F8C[i].r;
        r = D_80076F9C[i].r - from;
        delta = r;
        r = D_80076F8C[i].r + delta * D_80076FAC[i] / D_80076FBC[i];
        from = D_80076F8C[i].g;
        g = D_80076F9C[i].g - from;
        delta = g;
        g = D_80076F8C[i].g + delta * D_80076FAC[i] / D_80076FBC[i];
        from = D_80076F8C[i].b;
        b = D_80076F9C[i].b - from;
        delta = b;
        b = D_80076F8C[i].b + delta * D_80076FAC[i] / D_80076FBC[i];
        switch (i) {
        case 0:
            poly->r0 = r;
            poly->g0 = g;
            poly->b0 = b;
            break;
        case 1:
            poly->r1 = r;
            poly->g1 = g;
            poly->b1 = b;
            break;
        case 2:
            poly->r2 = r;
            poly->g2 = g;
            poly->b2 = b;
            break;
        case 3:
            poly->r3 = r;
            poly->g3 = g;
            poly->b3 = b;
            break;
        }
    }
    poly->tag = (poly->tag & 0xFF000000) | (*ot & 0xFFFFFF);
    *ot = (*ot & 0xFF000000) | ((u32)poly & 0xFFFFFF);
}

/* Set up the menu frame quads of both buffers at (x, y), w by h, with random
 * pale yellow corner fades. */
void func_80073328(POLY_G4 *poly0, POLY_G4 *poly1, s32 x, s32 y, s32 w, s32 h) {
    s32 i;

    for (i = 0; i < 4; i++) {
        D_80076FCC[i].r = 0xFF;
        D_80076FCC[i].g = 0xFF;
        D_80076FCC[i].b = (func_80074AF0() & 0x3F) - 0x42;
        D_80076FDC[i].r = 0xFF;
        D_80076FDC[i].g = 0xFF;
        D_80076FDC[i].b = (func_80074AF0() & 0x3F) - 0x42;
    }
    for (i = 0; i < 4; i++) {
        D_80076FEC[i] = 0;
        D_80076FFC[i] = (func_80074AF0() & 0xFF) + 32;
    }
    SetPolyG4(poly0);
    poly0->x0 = x;
    poly0->y0 = y;
    poly0->x1 = x + w;
    poly0->y1 = y;
    poly0->x2 = x;
    poly0->y2 = y + h;
    poly0->x3 = x + w;
    poly0->y3 = y + h;
    SetPolyG4(poly1);
    poly1->x0 = x;
    poly1->y0 = y;
    poly1->x1 = x + w;
    poly1->y1 = y;
    poly1->x2 = x;
    poly1->y2 = y + h;
    poly1->x3 = x + w;
    poly1->y3 = y + h;
}

/* Add the menu frame to `ot`: a gouraud quad at (x, y), w by h, whose corner
 * colors each fade toward a new random pale yellow. Each component is eased
 * from the byte `from` and added back to a fresh read of the from-colour. */
void func_800734B8(u_long *ot, POLY_G4 *poly, s32 x, s32 y, s32 w, s32 h) {
    s32 i;
    u8 from;
    s32 delta;
    s32 r;
    s32 g;
    s32 b;

    poly->x0 = x;
    poly->y0 = y;
    poly->y1 = y;
    poly->x2 = x;
    poly->x1 = x + w;
    poly->y2 = y + h;
    poly->x3 = x + w;
    poly->y3 = y + h;
    for (i = 0; i < 4; i++) {
        if (++D_80076FEC[i] > D_80076FFC[i]) {
            D_80076FEC[i] = 0;
            D_80076FFC[i] = (func_80074AF0() & 0xFF) + 32;
            D_80076FCC[i].r = D_80076FDC[i].r;
            D_80076FCC[i].g = D_80076FDC[i].g;
            D_80076FCC[i].b = D_80076FDC[i].b;
            D_80076FDC[i].r = 0xFF;
            D_80076FDC[i].g = 0xFF;
            D_80076FDC[i].b = (func_80074AF0() & 0x3F) - 0x42;
        }
        from = D_80076FCC[i].r;
        delta = (D_80076FDC[i].r - from) * D_80076FEC[i] / D_80076FFC[i];
        r = D_80076FCC[i].r + delta;
        from = D_80076FCC[i].g;
        delta = (D_80076FDC[i].g - from) * D_80076FEC[i] / D_80076FFC[i];
        g = D_80076FCC[i].g + delta;
        from = D_80076FCC[i].b;
        delta = (D_80076FDC[i].b - from) * D_80076FEC[i] / D_80076FFC[i];
        b = D_80076FCC[i].b + delta;
        switch (i) {
        case 0:
            poly->r0 = r;
            poly->g0 = g;
            poly->b0 = b;
            break;
        case 1:
            poly->r1 = r;
            poly->g1 = g;
            poly->b1 = b;
            break;
        case 2:
            poly->r2 = r;
            poly->g2 = g;
            poly->b2 = b;
            break;
        case 3:
            poly->r3 = r;
            poly->g3 = g;
            poly->b3 = b;
            break;
        }
    }
    poly->tag = (poly->tag & 0xFF000000) | (*ot & 0xFFFFFF);
    *ot = (*ot & 0xFF000000) | ((u32)poly & 0xFFFFFF);
}

/* Mode 6 entry. Load the movie library below the heap top and open it at
 * 320x256; with a movie request (8004fe44..47) play it and select the next
 * mode. Otherwise run the development menu: movie type, number, start and
 * end frame (Circle seeks them), channel, colour depth, rows drawn, rewind,
 * then the movie test, CD-ROM monitor, CD-ROM check, FAT check, disc change
 * test and a return to the kernel. Square and Cross speed up the frame
 * settings. The menu cursor is kept across the screens it opens. The unused
 * name reproduces the original's frame. The debug views are cleared and
 * 24-bit library output selected before the row and last-frame defaults:
 * stored in the order the binary shows (rows and last frame first), the
 * schedule differs. */
void func_800737EC(void) {
    char name[8] = "trouble";
    s32 button;
    s32 step;
    s32 dir;
    s32 line;
    s32 last;
    s32 cursor;
    void *top;
    void *library;
    MovieBuffer *buffer;
    u_long *ot;

    heap_select_owner_tag(4, 0);
    cd_select_directory(0x18, 0);
    sound_set_cd_volume(0, 0);
    DrawSync(0);
    VSync(0);
    SetDispMask(0);
    top = heap_alloc(4, 1);
    library = heap_alloc(((u32)top & 0xFFFFFF) - 0x1D3008, 1);
    heap_free(top);
    cd_read_file(1, library, 0, 0);
    cd_sync_reads(0);
    movie_open(320, 256, 128, 16, 32, 0x800, 3);
    D_80077450 = cd_has_pc_file_server();
    D_80077394 = 0;
    D_800773B0 = 0;
    D_80077454 = 1;
    D_800773A0 = -1;
    D_8007739C = 0xC80;
    D_80077448 = 1;
    D_8007711C = 0;
    D_800773A4 = 1;
    D_80077398 = 1;
    D_80077444 = 2;
    D_8007743C = 0;
    D_800773A8 = 0;
    D_80077438 = 0;
    SetDefDrawEnv(&D_80077124[0].draw, 0, 0, 320, 240);
    SetDefDispEnv(&D_80077124[0].disp, 0, 240, 320, 240);
    SetDefDrawEnv(&D_80077124[1].draw, 0, 240, 320, 240);
    SetDefDispEnv(&D_80077124[1].disp, 0, 0, 320, 240);
    D_80077124[0].draw.dtd = 1;
    D_80077124[0].draw.isbg = 1;
    D_80077124[1].draw.dtd = 1;
    D_80077124[1].draw.isbg = 1;
    D_80077124[0].draw.r0 = 0;
    D_80077124[0].draw.g0 = 0;
    D_80077124[0].draw.b0 = 0;
    D_80077124[0].disp.isinter = 0;
    D_80077124[1].draw.r0 = 0;
    D_80077124[1].draw.g0 = 0;
    D_80077124[1].draw.b0 = 0;
    D_80077124[1].disp.isinter = 0;
    D_80077124[0].disp.screen.x = 0;
    D_80077124[0].disp.screen.y = 10;
    D_80077124[0].disp.screen.w = 256;
    D_80077124[0].disp.screen.h = 216;
    D_80077124[1].disp.screen.x = 0;
    D_80077124[1].disp.screen.y = 10;
    D_80077124[1].disp.screen.w = 256;
    D_80077124[1].disp.screen.h = 216;
    D_80077120 = &D_80077124[0];
    D_8007744C = 0;
    PutDrawEnv(&D_80077124[0].draw);
    PutDispEnv(&D_80077120->disp);
    if (D_80077450 != 0) {
        VSync(2);
        D_800773AC = pad_read_buttons(0);
    } else {
        D_800773AC = 0;
    }
    if (D_8004FE44_request[0] != 0xFF && !(D_800773AC & 0x100)) {
        D_80077440 = 0;
        D_80077398 = 1;
        D_800773A4 = 1;
        D_80077448 = D_8004FE44_request[0] & 0x7F;
        D_8007711C = D_8004FE44_request[1];
        if (D_8004FE44_request[0] & 0x80) {
            D_8007739C = cd_movie_request_last_frame;
        } else {
            D_8007739C = 0xE9;
        }
        func_800763BC(D_8004FE44_request[3]);
        movie_close();
        heap_free(library);
        mode_select_next_mode(D_8004FE44_request[2]);
        mode_dispatch(0);
    }
    func_80072D84((POLY_G4 *)D_80077124[0].box, (POLY_G4 *)D_80077124[1].box, 0, 0, 0, 0);
    func_80073328((POLY_G4 *)D_80077124[0].frame, (POLY_G4 *)D_80077124[1].frame, 0, 0, 0, 0);
    console_open(16, 16, 640, 240, 0x400, 0, 640, 0, 640, 256, 0);
    D_80077440 = 1;
    SetDispMask(1);
    for (;;) {
        if (D_80077120 == &D_80077124[0]) {
            buffer = &D_80077124[1];
        } else {
            buffer = &D_80077124[0];
        }
        ot = buffer->ot;
        D_80077120 = buffer;
        D_8007744C = 1 - D_8007744C;
        ClearOTagR(ot, 32);
        if (D_80077450 == 0) {
            console_printf("  [ MOVIE CD-ROM MODE1 DISK %1d ]  \n\n", cd_get_disc_number());
        } else if (D_80077450 == -1) {
            console_printf("  [ MOVIE CD-ROM MODE2 DISK %1d ]  \n\n", cd_get_disc_number());
        } else {
            console_printf("  [ MOVIE PC HDD MODE  DISK %1d ]  \n\n", cd_get_disc_number());
        }
        console_printf("    ERROR %2d Sect %2d:%2d FM%3d\n", cd_error_count, cd_stat_stop_ok_count, cd_stat_stop_fail_count,
                      (s16)stream_frame_number);
        step = 1;
        console_printf("    LesMem%2d NoMem%2d Skp%3d\n", cd_stat_lesmem_count, cd_stat_error_limit_count,
                      movie_skipped_frames, cd_movie_request_last_frame);
        dir = func_800747AC(0, 13, &button);
        if (D_800773AC & 0x10) {
            step = 32;
        }
        if (D_800773AC & 0x80) {
            step <<= 7;
        }
        if (D_80077118 == 0 && dir != 0) {
            D_80077448 += dir;
            if (D_80077448 < 0) {
                D_80077448 = 2;
            }
            if (D_80077448 >= 3) {
                D_80077448 = 0;
            }
            D_800773A4 = 1;
            D_800773A8 = 0;
            D_8007743C = 0;
            D_80077444 = 2;
        }
        if (D_80077118 == 1 && dir != 0) {
            D_8007711C += dir;
            if (D_8007711C < 0) {
                D_8007711C = 63;
            }
            if (D_8007711C >= 64) {
                D_8007711C = 0;
            }
            D_800773A4 = 1;
            D_800773A8 = 0;
            D_8007743C = 0;
            D_80077444 = 2;
        }
        if (D_80077118 == 2 && dir != 0) {
            D_800773A4 += dir * step;
            if (D_800773A4 <= 0) {
                D_800773A4 = 1;
            }
            if (D_800773A4 >= 0x2000) {
                D_800773A4 = 0x1FFF;
            }
            if (D_800773A4 > D_8007739C) {
                D_800773A4 = D_8007739C;
            }
            D_80077444 = 1;
        }
        if (D_80077118 == 2 && button == 2 && D_80077444 == 1) {
            D_800773A8 = func_80074BA4(D_800773A4);
            if (D_800773A4 >= D_8007739C) {
                D_8007739C = D_800773A4;
                D_8007743C = 0;
            }
            D_80077444 = 2;
        }
        if (D_80077118 == 3 && dir != 0) {
            D_8007739C += dir * step;
            if (D_8007739C <= 0) {
                D_8007739C = 1;
            }
            if (D_8007739C >= 0x2000) {
                D_8007739C = 0x1FFF;
            }
            if (D_8007739C < D_800773A4) {
                D_8007739C = D_800773A4;
            }
            D_8007743C = 0;
        }
        if (D_80077118 == 3 && button == 2 && D_8007743C == 0) {
            last = func_8007519C();
            if (last >= 0) {
                D_8007739C = last;
                D_8007743C = 1;
                if (D_800773A4 >= last) {
                    D_800773A4 = last;
                    D_80077444 = 1;
                }
            } else {
                D_8007743C = 2;
            }
        }
        if (D_80077118 == 4 && dir != 0) {
            D_80077398 += dir;
            if (D_80077398 < 0) {
                D_80077398 = 7;
            }
            if (D_80077398 >= 8) {
                D_80077398 = 0;
            }
        }
        if (D_80077118 == 5 && dir != 0) {
            D_80077454 = 1 - D_80077454;
        }
        if (D_80077118 == 6) {
            if (dir != 0) {
                D_800773A0 = (D_800773A0 + dir * step) & 0xFF;
            }
            if (button == 2) {
                D_800773A0 = -1;
            }
        }
        if (D_80077118 == 7 && dir != 0) {
            D_80077438 = 1 - D_80077438;
        }
        for (line = 0; line < 14; line++) {
            console_printf(D_80077118 == line ? "  >" : "   ");
            switch (line) {
            case 0:
                console_printf(" MOVIE TYPE   ");
                if (D_80077448 == 0) {
                    console_printf("PICTURE ONLY\n");
                } else if (D_80077448 == 1) {
                    console_printf("PICTURE+ADPCM\n");
                } else if (D_80077448 == 2) {
                    console_printf("ADPCM ONLY\n");
                }
                break;
            case 1:
                console_printf(" MOVIE NUMBER %4d\n\n", D_8007711C);
                break;
            case 2:
                console_printf(" START FRAME  %4d ", D_800773A4);
                if (D_80077444 == 1) {
                    console_printf("SET");
                }
                if (D_80077444 == 2) {
                    if (D_800773A8 < 0) {
                        console_printf("EOF");
                    } else {
                        console_printf("+%4dSECT", D_800773A8);
                    }
                }
                console_printf("\n");
                break;
            case 3:
                console_printf(" END   FRAME  %4d ", D_8007739C);
                if (D_8007743C == 0) {
                    console_printf("SET");
                }
                if (D_8007743C == 2) {
                    console_printf("???");
                }
                console_printf("\n");
                break;
            case 4:
                console_printf(" MOVIE CHANNEL %3d\n", D_80077398);
                break;
            case 5:
                console_printf(" SCREEN MODE  ");
                console_printf(D_80077454 ? "24 BIT COLOR" : "16 BIT COLOR");
                console_printf("\n");
                break;
            case 6:
                console_printf(" SCREEN DRAW  ");
                if (D_800773A0 < 0) {
                    console_printf("ALL");
                } else {
                    console_printf("%3d", D_800773A0);
                }
                console_printf("\n");
                break;
            case 7:
                console_printf(" REWIND       ");
                console_printf(D_80077438 ? "ON" : "OFF");
                console_printf("\n\n");
                break;
            case 8:
                console_printf(" MOVIE START.\n\n");
                break;
            case 9:
                console_printf(" CD-ROM MONITOR.\n\n");
                break;
            case 10:
                console_printf(" CD-ROM CHECK.\n");
                break;
            case 11:
                console_printf(" FAT CHECK.\n\n");
                break;
            case 12:
                console_printf(" [DISC CHANGE.]\n");
                break;
            case 13:
                console_printf(" [RETURN TO KERNEL.]\n");
                break;
            }
        }
        if (D_80077394 > 0) {
            heap_print_report(1, 0, 6, 0x808D);
        }
        console_flush(D_80077120->ot);
        func_80072F98(D_80077120->ot, (POLY_G4 *)D_80077120->box, 20, 12, 284, 198);
        func_800734B8(D_80077120->ot, (POLY_G4 *)D_80077120->frame, 19, 11, 286, 200);
        DrawSync(0);
        VSync(0);
        PutDrawEnv(&D_80077120->draw);
        PutDispEnv(&D_80077120->disp);
        DrawOTag(&D_80077120->ot[31]);
        cursor = D_80077118;
        if ((cursor == 0 || cursor == 1 || cursor == 4 || cursor == 5 || cursor == 7 ||
             cursor == 8) &&
            button == 2) {
            cd_stat_lesmem_count = 0;
            cd_stat_error_limit_count = 0;
            cd_stat_stop_ok_count = 0;
            cd_stat_stop_fail_count = 0;
            func_8007625C();
            D_800773AC = -1;
        }
        if (D_80077118 == 9 && button == 2) {
            func_80075534();
        }
        if (D_80077118 == 10 && button == 2) {
            func_800704E8();
        }
        if (D_80077118 == 11 && button == 2) {
            func_80075D8C();
        }
        if (D_80077118 == 12 && button == 2) {
            func_80072480();
        }
        if (D_80077118 == 13 && button == 2) {
            heap_free(library);
            mode_dispatch(0);
        }
        D_80077118 = cursor;
        boot_check_soft_reset();
    }
}

/* Menu input: read controller port 0 with a repeat after 16 frames held; Up
 * and Down move the cursor between `first` and `last`, wrapping; the four
 * face buttons report 1..4 in `button`; Select toggles the monitor and Start
 * the statistics. Returns 1 for Right, -1 for Left, else 0. */
s32 func_800747AC(s32 first, s32 last, s32 *button) {
    D_800773B4 = D_800773AC;
    D_800773AC = pad_read_buttons(0);
    if (D_800773B4 == D_800773AC && D_800773B4 != 0) {
        D_80076EEC++;
        if (D_80076EF0 < D_80076EEC) {
            D_800773B4 = 0;
            D_80076EF0 = 1;
            D_80076EEC = 0;
        }
    } else {
        D_80076EF0 = 16;
        D_80076EEC = 0;
    }
    if (!(D_800773B4 & 0x1000) && (D_800773AC & 0x1000)) {
        if (--D_80077118 < first) {
            D_80077118 = last;
        }
    }
    if (!(D_800773B4 & 0x4000) && (D_800773AC & 0x4000)) {
        if (++D_80077118 > last) {
            D_80077118 = first;
        }
    }
    *button = 0;
    if (!(D_800773B4 & 0x10) && (D_800773AC & 0x10)) {
        *button = 1;
    }
    if (!(D_800773B4 & 0x20) && (D_800773AC & 0x20)) {
        *button = 2;
    }
    if (!(D_800773B4 & 0x40) && (D_800773AC & 0x40)) {
        *button = 3;
    }
    if (!(D_800773B4 & 0x80) && (D_800773AC & 0x80)) {
        *button = 4;
    }
    if (!(D_800773B4 & 0x100) && (D_800773AC & 0x100)) {
        D_800773B0 = 1 - D_800773B0;
    }
    if (!(D_800773B4 & 0x800) && (D_800773AC & 0x800)) {
        D_80077394 = 1 - D_80077394;
    }
    if (!(D_800773B4 & 0x2000) && (D_800773AC & 0x2000)) {
        return 1;
    }
    if (!(D_800773B4 & 0x8000) && (D_800773AC & 0x8000)) {
        return -1;
    }
    return 0;
}

/* Next pseudo-random number (two mixed linear congruential sequences). */
s32 func_80074AF0(void) {
    D_80076EF4 = D_80076EF4 * 5 + 1;
    D_80076EF8 = D_80076EF8 * 7 + 3;
    D_80076EF4 = (D_80076EF4 ^ D_80076EF8) + 1;
    if (D_80076EF4 < 0) {
        D_80076EF4 = -D_80076EF4;
    }
    return D_80076EF4;
}

/* Clear all of VRAM to black. */
void func_80074B58(void) {
    RECT rect;

    rect.x = 0;
    rect.y = 0;
    rect.w = 640;
    rect.h = 480;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
}

/* The sector of the selected movie where `frame` starts: guess from the
 * first sector's sectors per frame, correct once, then step sector by
 * sector (frame by frame on headers) until a header names the frame.
 * Returns 0 for the first frame, -1 past the file; an index past the list
 * returns without a value, as the original does. */
s32 func_80074BA4(s32 frame) {
    u8 buffer[0x1000];
    s32 file;
    char *name;
    s32 sector_size;
    s32 read_size;
    s32 header;
    s32 lower;
    s32 fd;
    s32 count;
    s32 per_frame;
    s32 pos;
    s32 next;
    s32 total;
    MovieSector *h;

    lower = 0;
    if (D_80077448 == 0) {
        cd_select_directory(0x18, 0);
        if (D_8007711C >= cd_get_directory_file_count(2)) {
            return;
        }
        file = D_8007711C + 3;
        name = cd_get_pc_file_name(file);
        sector_size = 0x800;
        read_size = 0x20;
        header = 0;
    } else if (D_80077448 == 1) {
        cd_select_directory(0x18, 1);
        if (D_8007711C >= cd_get_directory_file_count(1)) {
            return;
        }
        file = D_8007711C + 2;
        name = cd_get_pc_file_name(file);
        sector_size = 0x920;
        read_size = 0x28;
        header = 8;
    } else if (D_80077448 == 2) {
        return -1;
    }
    if (frame < 2) {
        return 0;
    }
    if (cd_has_pc_file_server() != 0) {
        fd = PCopen(name, 0, 0);
        PClseek(fd, 0, 2);
        PClseek(fd, 0, 0);
        PCread(fd, buffer, read_size);
        h = (MovieSector *)&buffer[header];
        per_frame = h->sectors;
        pos = (frame - 1) * per_frame - (frame - 1) / 4;
        PClseek(fd, pos * sector_size, 0);
        count = PCread(fd, buffer, read_size);
        if (h->frame == frame && count != 0) {
            if (h->sector == 0) {
                goto done;
            }
            pos -= h->sector;
            next = pos - 2;
        } else {
            if (h->frame < frame && lower < pos && count != 0) {
                lower = pos;
            }
            pos = (frame - 1) * per_frame - (frame - 1) / 4;
            pos += pos / 7;
            PClseek(fd, pos * sector_size, 0);
            count = PCread(fd, buffer, read_size);
            h = (MovieSector *)&buffer[header];
            if (h->frame == frame && count != 0) {
                if (h->sector == 0) {
                    goto done;
                }
                pos -= h->sector;
                next = pos - 2;
            } else {
                if (h->frame < frame && lower < pos && count != 0) {
                    lower = pos;
                }
                next = (per_frame - 1) * (frame - 1);
                if (lower > 0) {
                    next = lower;
                }
            }
        }
        do {
            pos = next;
            PClseek(fd, next * sector_size, 0);
            count = PCread(fd, buffer, read_size);
            h = (MovieSector *)&buffer[header];
            if (h->magic == 0x160) {
                next = pos + (h->sectors - h->sector);
            } else {
                next = pos + 1;
            }
        } while (h->frame != frame && count > 0);
        if (count == 0) {
            pos = -1;
        }
    done:
        PCclose(fd);
        return pos;
    }
    cd_read_raw_sectors(cd_get_file_sector(file), buffer, 0x800, 0, 0);
    cd_sync_reads(0);
    h = (MovieSector *)buffer;
    per_frame = h->sectors;
    total = (cd_get_aligned_file_size(file) + sector_size - 1) / sector_size;
    pos = (frame - 1) * per_frame - (frame - 1) / 4;
    cd_read_raw_sectors(cd_get_file_sector(file) + pos, buffer, 0x800, 0, 0);
    cd_sync_reads(0);
    if (h->frame == frame && pos < total) {
        if (h->sector == 0) {
            goto end;
        }
        pos -= h->sector;
        next = pos - 2;
    } else {
        if (h->frame < frame && lower < pos && pos < total) {
            lower = pos;
        }
        pos = (frame - 1) * per_frame - (frame - 1) / 4;
        pos += pos / 7;
        cd_read_raw_sectors(cd_get_file_sector(file) + pos, buffer, 0x800, 0, 0);
        cd_sync_reads(0);
        if (h->frame == frame && pos < total) {
            if (h->sector == 0) {
                goto end;
            }
            pos -= h->sector;
            next = pos - 2;
        } else {
            if (h->frame < frame && lower < pos && pos < total) {
                lower = pos;
            }
            next = (per_frame - 1) * (frame - 1);
            if (lower > 0) {
                next = lower;
            }
        }
    }
    do {
        pos = next;
        cd_read_raw_sectors(cd_get_file_sector(file) + pos, buffer, 0x800, 0, 0);
        cd_sync_reads(0);
        h = (MovieSector *)buffer;
        if (h->magic == 0x160) {
            next = pos + (h->sectors - h->sector);
        } else {
            next = pos + 1;
        }
    } while (h->frame != frame && next < total);
end:
    if (next >= total) {
        pos = -1;
    }
    return pos;
}

/* The last frame number of the selected movie, read from the header of its
 * last sector (host PC file or disc), or -1. An index past the list returns
 * without a value, and a kind above 2 reads with unset parameters, as the
 * original does. */
s32 func_8007519C(void) {
    u8 buffer[0x1000];
    s32 file;
    char *name;
    s32 sector_size;
    s32 read_size;
    s32 frames;
    s32 header;
    s32 fd;
    s32 size;

    frames = -1;
    if (D_80077448 == 0) {
        cd_select_directory(0x18, 0);
        if (D_8007711C >= cd_get_directory_file_count(2)) {
            return;
        }
        file = D_8007711C + 3;
        name = cd_get_pc_file_name(file);
        sector_size = 0x800;
        read_size = 0x20;
        header = 0;
    } else if (D_80077448 == 1) {
        cd_select_directory(0x18, 1);
        if (D_8007711C >= cd_get_directory_file_count(1)) {
            return;
        }
        file = D_8007711C + 2;
        name = cd_get_pc_file_name(file);
        sector_size = 0x920;
        read_size = 0x28;
        header = 8;
    } else if (D_80077448 == 2) {
        return -1;
    }
    if (cd_has_pc_file_server() != 0) {
        fd = PCopen(name, 0, 0);
        size = PClseek(fd, 0, 2);
        PClseek(fd, 0, 0);
        PClseek(fd, size - sector_size, 0);
        PCread(fd, buffer, read_size);
        if (((MovieSector *)(buffer + header))->magic == 0x160) {
            frames = ((MovieSector *)(buffer + header))->frame;
        }
        PCclose(fd);
    } else {
        size = (cd_get_aligned_file_size(file) + sector_size - 1) / sector_size;
        cd_read_raw_sectors(cd_get_file_sector(file) + size - 1, buffer, 0x800, 0, 0);
        cd_sync_reads(0);
        if (((MovieSector *)buffer)->magic == 0x160) {
            frames = ((MovieSector *)buffer)->frame;
        }
    }
    return frames;
}

/* Load the three sound effect banks from the host PC, waiting for each
 * transfer to the sound memory. */
void func_800753B8(void) {
    void *bank;

    bank = cd_load_pc_file("c:\\work\\cdrom\\sound\\wave\\main_se.wd", 0);
    sound_load_wave_bank(bank, 0);
    while (sound_sync_transfer(0) != 0) {
    }
    heap_free(bank);
    bank = cd_load_pc_file("c:\\work\\cdrom\\sound\\wave\\bat_se.wd", 0);
    sound_load_wave_bank(bank, 0);
    while (sound_sync_transfer(0) != 0) {
    }
    heap_free(bank);
    bank = cd_load_pc_file("c:\\work\\cdrom\\sound\\wave\\gear_se.wd", 0);
    sound_load_wave_bank(bank, 0);
    while (sound_sync_transfer(0) != 0) {
    }
    heap_free(bank);
}

/* Two strings of this unit linked as original rodata below their users
 * (INCLUDE_RODATA, each after its function). */
extern char D_80070394[];
extern char D_800704D4[];

/* Load the battle sound bank from the host PC, waiting for its transfer,
 * then the battle music sequence. */
void func_8007548C(void) {
    void *bank;

    bank = cd_load_pc_file("c:\\work\\cdrom\\sound\\wave\\battle2.wd", 0);
    sound_load_wave_bank(bank, 0);
    while (sound_sync_transfer(0) != 0) {
    }
    heap_free(bank);
    D_8007700C = sound_create_seq(cd_load_pc_file(D_80070394, 0));
}

/* "c:\\work\\cdrom\\sound\\music\\battle2.smd". The original assembler left a
 * stray byte (0x08) in its alignment padding; it is linked as original rodata. */
INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_80070394);

/* Play the battle music sequence (8007548c loads it) from its start at full
 * volume. */
void func_80075508(void) {
    sound_play_seq(D_8007700C, 0x7F, 0);
}

/* The menu's sector monitor: at 640x240, dump 192 bytes of the current
 * sector (Up/Down by a row, Triangle/Cross by twelve) with its position;
 * Left/Right step the sector by one, L1/R1 by 75 (a second) and L2/R2 by
 * 4500 (a minute), rereading it when it changes. Circle returns to the
 * 320-wide menu. The original reads the menu cursor before the exit test and
 * stores it back after. */
void func_80075534(void) {
    CdlLOC loc;
    s32 button;
    u8 *buffer;
    s32 sector;
    s32 row;
    s32 i;
    u8 *hex;
    u8 *text;
    u8 c;
    s32 cursor;
    MovieBuffer *draw;
    u_long *ot;

    func_80074B58();
    buffer = heap_alloc(0x800, 0);
    if (buffer == NULL) {
        return;
    }
    SetDefDrawEnv(&D_80077124[0].draw, 0, 0, 640, 240);
    SetDefDispEnv(&D_80077124[0].disp, 0, 240, 640, 240);
    SetDefDrawEnv(&D_80077124[1].draw, 0, 240, 640, 240);
    SetDefDispEnv(&D_80077124[1].disp, 0, 0, 640, 240);
    D_80077124[0].draw.isbg = 1;
    D_80077124[1].draw.isbg = 1;
    D_80077124[0].disp.screen.x = 0;
    D_80077124[0].disp.screen.y = 10;
    D_80077124[0].disp.screen.w = 256;
    D_80077124[0].disp.screen.h = 216;
    D_80077124[1].disp.screen.x = 0;
    D_80077124[1].disp.screen.y = 10;
    D_80077124[1].disp.screen.w = 256;
    D_80077124[1].disp.screen.h = 216;
    cd_read_raw_sectors(D_80076EFC, buffer, 0x800, 0, 0);
    cd_sync_reads(0);
    for (;;) {
        if (D_80077120 == &D_80077124[0]) {
            draw = &D_80077124[1];
        } else {
            draw = &D_80077124[0];
        }
        ot = draw->ot;
        D_80077120 = draw;
        D_8007744C = 1 - D_8007744C;
        ClearOTagR(ot, 32);
        console_printf("\n[ MONITOR ]\n");
        func_800747AC(0, 0, &button);
        sector = D_80076EFC;
        if (!(D_800773B4 & 0x1000) && (D_800773AC & 0x1000) && D_80076F00 > 0) {
            D_80076F00--;
        }
        if (!(D_800773B4 & 0x10) && (D_800773AC & 0x10)) {
            D_80076F00 -= 12;
            if (D_80076F00 < 0) {
                D_80076F00 = 0;
            }
        }
        if (!(D_800773B4 & 0x4000) && (D_800773AC & 0x4000) && D_80076F00 < 116) {
            D_80076F00++;
        }
        if (!(D_800773B4 & 0x40) && (D_800773AC & 0x40)) {
            D_80076F00 += 12;
            if (D_80076F00 >= 116) {
                D_80076F00 = 116;
            }
        }
        if (!(D_800773B4 & 0x8000) && (D_800773AC & 0x8000) && D_80076EFC > 0) {
            D_80076EFC--;
        }
        if (!(D_800773B4 & 0x2000) && (D_800773AC & 0x2000)) {
            D_80076EFC++;
        }
        if (!(D_800773B4 & 4) && (D_800773AC & 4)) {
            D_80076EFC -= 75;
            if (D_80076EFC < 0) {
                D_80076EFC = 0;
            }
        }
        if (!(D_800773B4 & 8) && (D_800773AC & 8)) {
            D_80076EFC += 75;
        }
        if (!(D_800773B4 & 1) && (D_800773AC & 1)) {
            D_80076EFC -= 4500;
            if (D_80076EFC < 0) {
                D_80076EFC = 0;
            }
        }
        if (!(D_800773B4 & 2) && (D_800773AC & 2)) {
            D_80076EFC += 4500;
        }
        if (sector != D_80076EFC) {
            cd_read_raw_sectors(D_80076EFC, buffer, 0x800, 0, 0);
            cd_sync_reads(0);
        }
        CdIntToPos(D_80076EFC, &loc);
        console_printf("ABSPOS %8d POS %04x\nMINUTE %02x SECOND %02x SECTOR %02x\n\n", D_80076EFC,
                      D_80076F00 * 16, loc.minute, loc.second, loc.sector);
        text = buffer + D_80076F00 * 16;
        hex = text;
        for (row = 0; row < 12; row++) {
            console_printf("%03x:", (row + D_80076F00) * 16);
            for (i = 0; i < 15; i++) {
                console_printf("%02x ", *hex);
                hex++;
            }
            console_printf("%02x", *hex);
            hex++;
            console_printf(":");
            for (i = 0; i < 16; i++) {
                c = *text;
                if (c >= 0x20 && c < 0x7E) {
                    console_printf("%c", c);
                } else {
                    console_printf(" ");
                }
                text++;
            }
            console_printf((char *)D_8007042C);
        }
        console_printf((char *)D_80070430);
        if (D_80077394 > 0) {
            heap_print_report(1, 0, 6, 0x808D);
        }
        console_flush(D_80077120->ot);
        func_80072F98(D_80077120->ot, (POLY_G4 *)D_80077120->box, 8, 20, 624, 160);
        func_800734B8(D_80077120->ot, (POLY_G4 *)D_80077120->frame, 7, 19, 626, 162);
        DrawSync(0);
        VSync(0);
        PutDrawEnv(&D_80077120->draw);
        PutDispEnv(&D_80077120->disp);
        DrawOTag(&D_80077120->ot[31]);
        cursor = D_80077118;
        if (button == 2) {
            break;
        }
        D_80077118 = cursor;
    }
    heap_free(buffer);
    func_80074B58();
    SetDefDrawEnv(&D_80077124[0].draw, 0, 0, 320, 240);
    SetDefDispEnv(&D_80077124[0].disp, 0, 240, 320, 240);
    SetDefDrawEnv(&D_80077124[1].draw, 0, 240, 320, 240);
    SetDefDispEnv(&D_80077124[1].disp, 0, 0, 320, 240);
    D_80077124[0].draw.isbg = 1;
    D_80077124[1].draw.isbg = 1;
    D_80077124[0].disp.screen.x = 0;
    D_80077124[0].disp.screen.y = 10;
    D_80077124[0].disp.screen.w = 256;
    D_80077124[0].disp.screen.h = 216;
    D_80077124[1].disp.screen.x = 0;
    D_80077124[1].disp.screen.y = 10;
    D_80077124[1].disp.screen.w = 256;
    D_80077124[1].disp.screen.h = 216;
}

/* Strings of the monitor (func_80075534) and the FAT check (func_80075D8C):
 * arrays, the "\n" a copy of D_8006FC6C's. */
const char D_8007042C[] = "\n";
const char D_80070430[] = "\nPUSH CIRCLE BUTTON TO MENU.";

/* The first sector of directory record `index` (bytes 3..6). */
u32 func_80075D4C(s32 index) {
    u8 *record;

    record = cd_file_index + index * 7;
    return ((record[6] << 24) + (record[5] << 16) + (record[4] << 8)) | record[3];
}

/* The menu's FAT check: list twenty directory records from the cursor
 * (Up/Down by one, Triangle/Cross by twenty) with their first sector and
 * their size or, toggled by L1, their host file name; R1 switches between
 * decimal and hexadecimal. Circle returns to the menu. */
void func_80075D8C(void) {
    s32 directory;
    s32 offset;
    s32 button;
    s32 top;
    s32 hex;
    s32 names;
    s32 index;
    s32 count;
    char *name;
    char *p;
    MovieBuffer *buffer;
    u_long *ot;

    func_80074B58();
    top = 0;
    cd_get_selected_directory(&directory, &offset);
    cd_select_directory(0, 0);
    hex = 0;
    names = 0;
    do {
        count = 0x1249;
        if (D_80077120 == &D_80077124[0]) {
            buffer = &D_80077124[1];
        } else {
            buffer = &D_80077124[0];
        }
        ot = buffer->ot;
        D_80077120 = buffer;
        D_8007744C = 1 - D_8007744C;
        ClearOTagR(ot, 32);
        console_printf("\n[ FAT CHECK MODE ");
        console_printf(hex ? "HEX ]\n" : "DEC ]\n");
        func_800747AC(0, 0, &button);
        if (!(D_800773B4 & 0x1000) && (D_800773AC & 0x1000) && top > 0) {
            top--;
        }
        if (!(D_800773B4 & 0x10) && (D_800773AC & 0x10)) {
            top -= 20;
            if (top < 0) {
                top = 0;
            }
        }
        if (!(D_800773B4 & 0x4000) && (D_800773AC & 0x4000) && top < count - 20) {
            top++;
        }
        if (!(D_800773B4 & 0x40) && (D_800773AC & 0x40)) {
            top += 20;
            if (top > count - 20) {
                top = count - 20;
            }
        }
        if (!(D_800773B4 & 8) && (D_800773AC & 8)) {
            hex = 1 - hex;
        }
        if (!(D_800773B4 & 4) && (D_800773AC & 4)) {
            names = 1 - names;
        }
        index = top;
        do {
            if (func_80075D4C(index) == 0) {
                if (hex) {
                    console_printf("No %4x NullFile\n", index);
                    index++;
                } else {
                    console_printf("No %4d NullFile\n", index);
                    index++;
                }
            } else {
                if (hex) {
                    console_printf("No %4x Sect%6x ", index, cd_get_file_sector(index + 1));
                } else {
                    console_printf("No %4d Sect%6d ", index, cd_get_file_sector(index + 1));
                }
                if (names) {
                    if ((s32)func_80075D4C(index) < 0) {
                        console_printf("[P%3d]\n", -func_80075D4C(index));
                        index++;
                    } else {
                        name = cd_get_pc_file_name(index + 1);
                        if (name != NULL) {
                            if (*name != 0) {
                                p = name;
                                do {
                                    if (*p == '\\') {
                                        name = p + 1;
                                    }
                                    p++;
                                } while (*p != 0);
                            }
                            console_printf("%s\n", name);
                        } else {
                            console_printf((char *)D_8007042C);
                        }
                        index++;
                    }
                } else if (hex) {
                    console_printf("Size%9x\n", func_80075D4C(index));
                    index++;
                } else {
                    console_printf((char *)D_800704D4, func_80075D4C(index));
                    index++;
                }
            }
        } while (index < top + 20);
        console_printf((char *)D_80070430);
        if (D_80077394 > 0) {
            heap_print_report(1, 0, 6, 0x808D);
        }
        console_flush(D_80077120->ot);
        func_80072F98(D_80077120->ot, (POLY_G4 *)D_80077120->box, 8, 20, 304, 192);
        func_800734B8(D_80077120->ot, (POLY_G4 *)D_80077120->frame, 7, 19, 306, 194);
        DrawSync(0);
        VSync(0);
        PutDrawEnv(&D_80077120->draw);
        PutDispEnv(&D_80077120->disp);
        DrawOTag(&D_80077120->ot[31]);
    } while (button != 2);
    cd_select_directory(directory, offset);
}

/* "Size%9d\n". The original assembler left stray bytes (0x94, 0x08) in its
 * alignment padding; it is linked as original rodata. */
INCLUDE_RODATA(".local/decomp/movie/asm/nonmatchings/movie", D_800704D4);

/* The menu's movie test: play the selected movie with the display blanked
 * until it starts, then clear the screen and restore the menu's buffers.
 * The unused array reproduces the original's frame. */
void func_8007625C(void) {
    u8 unused[0x18];
    RECT screen;

    screen = D_800704E0;
    D_80077028 = 0;
    D_800773AC = -1;
    if (D_80077448 == 0) {
        cd_select_directory(0x18, 0);
        if (D_8007711C >= cd_get_directory_file_count(2)) {
            return;
        }
        SetDispMask(0);
    } else if (D_80077448 == 1) {
        cd_select_directory(0x18, 1);
        if (D_8007711C >= cd_get_directory_file_count(1)) {
            return;
        }
        SetDispMask(0);
    } else if (D_80077448 == 2) {
        return;
    }
    D_80077124[0].draw.isbg = 0;
    D_80077124[1].draw.isbg = 0;
    if (D_80077454 != 0) {
        D_80077124[0].disp.isrgb24 = 1;
        D_80077124[1].disp.isrgb24 = 1;
    }
    func_80076488();
    VSync(0);
    ClearImage(&screen, 0, 0, 0);
    DrawSync(0);
    VSync(0);
    D_80077124[0].draw.isbg = 1;
    D_80077124[1].draw.isbg = 1;
    if (D_80077454 != 0) {
        D_80077124[0].disp.isrgb24 = 0;
        D_80077124[1].disp.isrgb24 = 0;
    }
}

/* Play the requested movie when its directory list holds it; `keep` stops
 * the buttons from ending it. Declared int without a value, as the original
 * (its return register stays live on every path). */
s32 func_800763BC(u8 keep) {
    u8 unused[0x18];

    D_80077028 = keep;
    D_800773AC = -1;
    if (D_80077448 == 0) {
        cd_select_directory(0x18, 0);
        if (D_8007711C >= cd_get_directory_file_count(2)) {
            return;
        }
    } else if (D_80077448 == 1) {
        cd_select_directory(0x18, 1);
        if (D_8007711C >= cd_get_directory_file_count(1)) {
            return;
        }
    } else if (D_80077448 == 2) {
        return;
    }
    D_80077124[0].draw.isbg = 0;
    D_80077124[1].draw.isbg = 0;
    D_80077124[0].disp.isrgb24 = 1;
    D_80077124[1].disp.isrgb24 = 1;
    func_80076488();
}

/* Clear the screen, reopen the movie library and stream the movie, running
 * three decode steps per frame (their VSync counters are kept for the
 * monitor) until the frame callback or a button ends it; a movie that ended
 * on the second buffer is copied to the first. The short-file exit falls
 * off the end without a return value, as in the original. */
s32 func_80076488(void) {
    u8 unused0[0x90]; /* unused in the original; reserves 144 bytes */
    RECT screen;
    u8 unused1[0x200]; /* unused in the original; reserves 512 bytes */
    RECT copy;
    s32 file;
    s32 select;
    s32 budget;
    s32 i;
    s32 before;
    s32 after;

    screen = D_800704E0;
    D_80077014 = 0;
    D_80077020 = 0;
    if (D_80077448 == 0) {
        select = 0;
        file = D_8007711C + 3;
    } else {
        select = 1;
        file = D_8007711C + 2;
    }
    if (cd_get_file_size(file) == 0x18) {
        SetDispMask(1);
        D_8004FE44_request[2] = 0;
    } else {
        VSync(0);
        ClearImage(&screen, 0, 0, 0);
        DrawSync(0);
        VSync(0);
        D_80077018 = 0;
        D_8007701C = 0;
        movie_close();
        if (D_800773A0 > 0) {
            D_80077024 = (240 - D_800773A0) / 2;
        } else {
            D_80077024 = 0;
        }
        movie_open(320, 240, 0x80, 16, 32, 0x800, D_80077454);
        movie_split_display = 0;
        if (D_80077438 != 0) {
            movie_start(file, D_800773A8, D_800773A4, D_8007739C, D_80077398, select, 1, 0,
                        D_80077024, 0, D_80077024 + 240, D_800773A0, func_800768D8);
        } else {
            movie_start(file, D_800773A8, D_800773A4, D_8007739C, D_80077398, select, 0, 0,
                        D_80077024, 0, D_80077024 + 240, D_800773A0, func_800768D8);
        }
        budget = 30;
        PutDrawEnv(&D_80077124[D_80077018].draw);
        PutDispEnv(&D_80077124[D_80077018].disp);
        SetDispMask(1);
        while (1) {
            if (D_80077020 == 0) {
                for (i = 0; i < budget / 10; i++) {
                    before = VSync(1);
                    movie_poll();
                    after = VSync(1);
                    if (i * 2 + 1 < 32) {
                        D_800773B8[i * 2] = before;
                        D_800773B8[i * 2 + 1] = after;
                    }
                }
                budget %= 10;
            }
            budget += 30;
            func_800769A4();
            boot_check_soft_reset();
            VSync(0);
            PutDispEnv(&D_80077124[D_8007701C].disp);
            D_8007701C = D_80077018;
            if (D_80077014 == 1) {
                break;
            }
            if (D_80077014 >= 2) {
                D_80077014--;
            }
        }
        movie_stop();
        if (D_8007701C == 0) {
            DrawSync(0);
            VSync(0);
            copy.x = 0;
            copy.y = 240;
            copy.w = 480;
            copy.h = 240;
            MoveImage(&copy, 0, 0);
            DrawSync(0);
            VSync(0);
            PutDispEnv(&D_80077124[1].disp);
        }
        return 0;
    }
}

/* The movie library's frame callback: the buffer the frame went to; the
 * last frame ends the movie. */
void func_800768D8(u16 frame, u16 x, u16 y) {
    D_80077010 = frame;
    if (movie_split_display == 1) {
        if (y != D_80077024) {
            D_80077018 = 0;
        } else {
            D_80077018 = 1;
        }
    } else if (y != D_80077024) {
        D_80077018 = 0;
        D_8007701C = 0;
    } else {
        D_80077018 = 1;
        D_8007701C = 1;
    }
    if (frame >= D_8007739C && D_80077438 == 0) {
        D_80077014 = 1;
    }
}

/* Read controller port 0 with a repeat after 90 frames held; Circle or Start
 * ends the movie in five frames, fading the CD volume, unless it is kept. */
void func_800769A4(void) {
    D_800773B4 = D_800773AC;
    D_800773AC = pad_read_buttons(0);
    if (D_800773B4 == D_800773AC && D_800773B4 != 0) {
        D_80076F04++;
        if (D_80076F08 < D_80076F04) {
            D_800773B4 = 0;
            D_80076F08 = 1;
            D_80076F04 = 0;
        }
    } else {
        D_80076F08 = 90;
        D_80076F04 = 0;
    }
    if (!(D_800773B4 & 0x40) && (D_800773AC & 0x40) && D_80077028 == 0) {
        sound_set_cd_volume(0, 10);
        D_80077014 = 5;
    }
    if (!(D_800773B4 & 0x800) && (D_800773AC & 0x800) && D_80077028 == 0) {
        sound_set_cd_volume(0, 10);
        D_80077014 = 5;
    }
}

/* Set up the geometry screen and the light and color matrices. */
void func_80076AF0(void) {
    SetGeomScreen(512);
    SetGeomOffset(160, 120);
    D_800770B8.m[0][0] = 0x1000;
    D_800770B8.m[1][1] = 0x1000;
    D_800770B8.m[2][2] = 0x1000;
    D_800770F8.m[0][0] = 0x1000;
    D_800770F8.m[1][1] = 0x1000;
    D_800770F8.m[2][2] = 0x1000;
    D_80077090.m[0][0] = 0x93D;
    D_80077090.m[0][1] = -0x93D;
    D_80077090.m[0][2] = -0x93D;
    D_800770B8.m[0][1] = 0;
    D_800770B8.m[0][2] = 0;
    D_800770B8.m[1][0] = 0;
    D_800770B8.m[1][2] = 0;
    D_800770B8.m[2][0] = 0;
    D_800770B8.m[2][1] = 0;
    D_800770F8.m[0][1] = 0;
    D_800770F8.m[0][2] = 0;
    D_800770F8.m[1][0] = 0;
    D_800770F8.m[1][2] = 0;
    D_800770F8.m[2][0] = 0;
    D_800770F8.m[2][1] = 0;
    D_80077090.m[1][0] = 0;
    D_80077090.m[1][1] = 0;
    D_80077090.m[1][2] = 0;
    D_80077090.m[2][0] = 0;
    D_80077090.m[2][1] = 0;
    D_80077090.m[2][2] = 0;
    D_80077070.m[0][0] = 0x969;
    D_80077070.m[0][1] = 0;
    D_80077070.m[0][2] = 0;
    D_80077070.m[1][0] = 0x969;
    D_80077070.m[1][1] = 0;
    D_80077070.m[1][2] = 0;
    D_80077070.m[2][0] = 0x969;
    D_80077070.m[2][1] = 0;
    D_80077070.m[2][2] = 0;
    SetColorMatrix(&D_80077070);
    SetBackColor(0x60, 0x60, 0x60);
}

/* Update the camera and load it as the rotation and translation. The unused
 * local reproduces the original's frame. */
void func_80076C68(void) {
    u8 unused[0x18];

    func_80076CA4();
    SetRotMatrix(&D_80077050);
    SetTransMatrix(&D_80077050);
}

/* Aim the camera from the eye at the target, rolled, and compose the world
 * to screen matrix. */
void func_80076CA4(void) {
    s32 dz;
    s32 dx;
    s32 dy;
    s32 dz2;
    s32 dx2;

    dx = D_8007703C.vx - D_8007702C.vx;
    dz = D_8007703C.vz - D_8007702C.vz;
    dz2 = dz * dz;
    dx2 = dx * dx;
    dy = D_8007703C.vy - D_8007702C.vy;
    D_800770B0.vx = ratan2(dy, SquareRoot0(dz2 + dx2));
    D_800770B0.vy = -ratan2(dx, dz);
    gpu_build_rotation_matrix(&D_800770B0, &D_800770D8);
    RotMatrixZ(D_8007704C, &D_800770D8);
    MulMatrix2(&D_800770F8, &D_800770D8);
    D_800770D8.t[0] = 0;
    D_800770D8.t[1] = 0;
    D_800770D8.t[2] = SquareRoot0(dx2 + dy * dy + dz2);
    D_800770B8.t[0] = D_80076F2C.vx - D_8007703C.vx;
    D_800770B8.t[1] = D_80076F2C.vy - D_8007703C.vy;
    D_800770B8.t[2] = D_80076F2C.vz - D_8007703C.vz;
    CompMatrix(&D_800770D8, &D_800770B8, &D_80077050);
}

const RECT D_800704E0 = {0, 0, 640, 512};
