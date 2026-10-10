#ifndef SLOT39_FILE_H
#define SLOT39_FILE_H

#include "common.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/gamedata.h"
#include "menu/tables.h"

/* The menu overlay's memory card and file screen (slot39): the card scan
 * and messages, save and load (the save file layout and the game data it
 * copies), the file screen's slots and views of a save, and the disc checks. */

/* The summary at the start of a save payload, which 801cba4c writes: the
 * play time and, per party slot, the character and its HP, EP and levels.
 * The file screen shows it from each listed file's first block (+100,
 * 801e76ec), a view per party slot. */
typedef struct SaveSummary {
    s32 time;     /* 0: play time in frames */
    u16 hp[3];    /* 4: per party slot */
    u16 hpMax[3]; /* A */
    u8 ep[3];     /* 10 */
    u8 epMax[3];  /* 13 */
    u8 level[3];  /* 16: the character's level (+62) */
    u8 level2[3]; /* 19: and its +63 */
    u8 ids[3];    /* 1C: the character, ff empty; the view shows sheet image 14e + id */
    u8 unk1F;     /* 1F: written 0 */
    u8 pad20[0x3];
    u8 digit;     /* 23: the file digit, shown plus one as two digits */
} SaveSummary;

/* A name of the save's name table (two-byte text). */
typedef struct MenuSaveName {
    u8 text[0x14];
} MenuSaveName;

/* The start of a save payload as the file screen reads it from a listed
 * file's first block (+100): the summary, then the characters' names. */
typedef struct MenuSaveInfo {
    SaveSummary summary;
    MenuSaveName names[11]; /* 24: game data 0, encoded */
} MenuSaveInfo;

/* A save information view (801e76ec). */
typedef struct MenuView {
    POLY_FT4 image[2]; /* 0: the entry's sheet image (801e6ae8) */
    POLY_FT4 frame[9][2]; /* 50: frame sprite list */
    POLY_FT4 levelDigits[6][2]; /* 320: digit sprite lists of the summary's level and (from 3) its +63 */
    POLY_FT4 hpDigits[3][2]; /* 500: of its HP */
    POLY_FT4 hpMaxDigits[3][2]; /* 5F0: of its maximum HP */
    POLY_FT4 epDigits[2][2]; /* 6E0: of its EP */
    POLY_FT4 epMaxDigits[2][2]; /* 780: of its maximum EP */
    POLY_FT4 name[2]; /* 820: the entry's name image */
    u8 levelCount; /* 870 */
    u8 level2Count; /* 871: drawn, but only ever reset */
    u8 hpCount; /* 872 */
    u8 hpMaxCount; /* 873 */
    u8 epCount; /* 874 */
    u8 epMaxCount; /* 875 */
    u8 frameBuffer; /* 876 */
    u8 buffer; /* 877 */
    u8 shown; /* 878 */
    u8 nameBuffer; /* 879 */
    u8 frameCount; /* 87A */
    u8 pad87B[0x1];
} MenuView;

/* The 2dc0-byte block (*(state + 34c)). */
typedef struct MenuFileInfo {
    POLY_FT4 chars[64]; /* 0: 32 text character quads, pairs per buffer */
    POLY_FT4 cursor[2]; /* A00: 32x32 sprite, per buffer */
    POLY_G4 band[2]; /* A50: shaded band, per buffer */
    MenuView views[3]; /* A98 */
    POLY_FT4 colon0[4]; /* 240C: play time separators */
    POLY_FT4 colon1[4]; /* 24AC */
    POLY_FT4 timeDigits[7][2]; /* 254C: play time digits, per buffer */
    POLY_FT4 title[32]; /* 277C: save title characters, pairs per buffer */
    POLY_FT4 discLabel[2]; /* 2C7C */
    POLY_FT4 discMark[2]; /* 2CCC */
    POLY_FT4 discDigits[2][2]; /* 2D1C: two digits */
    u8 rebuilt; /* 2DBC */
    u8 pad2DBD[0x3];
} MenuFileInfo;

/* The card access indicator (*(state + 44c), 7bc bytes). */
typedef struct MenuIndicator {
    POLY_F4 fills[2]; /* 0: per buffer */
    POLY_FT4 spriteA[24]; /* 30: sprite 160 */
    POLY_FT4 spriteB[24]; /* 3F0: sprite 161 */
    s32 unk7B0; /* 7B0 */
    s32 unk7B4; /* 7B4 */
    u8 buffer; /* 7B8: buffer it was built for */
    u8 pad7B9[0x3];
} MenuIndicator;

/* A file screen slot (*(state + 3a8 + 4 * slot)): its icon, the two
 * connector lines to the next slot and the cursor box, one per buffer. The
 * item and equipment screens draw the lines as the icon's frame and the box
 * as a semi-transparent cover. */
typedef struct MenuSlotImage {
    POLY_FT4 icon[2];  /* 0 */
    LINE_F3 lineA[2];  /* 50 */
    LINE_F3 lineB[2];  /* 80 */
    POLY_F4 box[2];    /* B0 */
    SVECTOR iconAt[4]; /* E0: also the cursor box */
    SVECTOR lineAAt[4]; /* 100 */
    SVECTOR lineBAt[4]; /* 120 */
    DR_MODE boxMode[2]; /* 140 */
} MenuSlotImage;

/* The disc label: the first 16 bytes of sector 0x17 (menu_cd_check_disc). */
typedef struct DiscLabel {
    u8 unk0[3];
    u8 disc; /* 3: '1' or '2' */
    s32 tag; /* 4: "_XEN" */
    u8 unk8[8];
} DiscLabel;

/* Word views of the game data blocks a save file copies whole. */
typedef struct SaveWords10 { s32 w[0x10 / 4]; } SaveWords10;

typedef struct SaveWords18 { s32 w[0x18 / 4]; } SaveWords18;

typedef struct SaveWords78 { s32 w[0x78 / 4]; } SaveWords78;

typedef struct SaveWordsA4 { s32 w[0xA4 / 4]; } SaveWordsA4;

typedef struct SaveWordsDC { s32 w[0xDC / 4]; } SaveWordsDC;

typedef struct SaveWords100 { s32 w[0x100 / 4]; } SaveWords100;

typedef struct SaveWords160 { s32 w[0x160 / 4]; } SaveWords160;

typedef struct SaveWords190 { s32 w[0x190 / 4]; } SaveWords190;

/* A gear as a save file keeps it (3c bytes). */
typedef struct SaveGear {
    SaveWords10 head;     /* 0: the gear record's first 0x10 bytes */
    SaveWords18 entries;  /* 10: its part entries (0x10) */
    s32 variants;         /* 28: fileVariant and spriteVariants (0x5c) */
    u32 hp;               /* 2C */
    u8 pad30[0x4];
    u16 fuel;             /* 34 */
    u8 pad36[0x2];
    u8 defense;           /* 38 */
    u8 field74;           /* 39 */
    u8 field75;           /* 3A */
    u8 pad3B[0x1];
} SaveGear;

/* The game data of a save file (after its header). */
typedef struct SaveData {
    SaveSummary summary; /* 0 */
    SaveWordsDC names; /* 24: game data 0 */
    SaveWords190 unk100; /* 100: game data dc */
    SaveWordsA4 chars[11]; /* 290: character records */
    SaveGear gears[20]; /* 99C */
    SaveWords78 unkE4C; /* E4C: game data 1648 */
    SaveWords160 records; /* EC4: game data 16c0 */
    SaveWords100 unk1024; /* 1024: game data 1820 */
    u8 unk1124[0xA38]; /* 1124: game data 1920 */
} SaveData;

/* The 31 names at the start of the game data (encoded in the save). */
#define GAME_NAMES ((u8 *)&game_data)

/* The battle script variables (a resident common): the save keeps them
 * in its flag words. No shared header declares them; battle reads them
 * signed. */
extern u16 mode_battle_ai_variables[16];
extern u8 menu_yes_no_cancelled;    /* the last choice was cancelled */
extern u16 menu_ascii_to_sjis_table[96]; /* two-byte codes of the ASCII characters 0x20-0x7F */
extern s16 menu_file_slot_x_table[32][2];    /* image block x */
extern s16 menu_file_slot_y_table[32][2];    /* image block y */
extern s32 menu_save_view_image_x_table[];
extern s32 menu_save_view_image_y_table[];
extern s32 menu_file_info_char_x_table[21];    /* text character x per column */
extern s32 menu_file_info_char_y_table[];      /* text character y per row */
extern s32 menu_file_info_cursor_x;        /* cursor sprite x */
extern s32 menu_file_info_cursor_y;        /* cursor sprite y */
extern s32 menu_save_view_frame_images[18];    /* view frame images, ffff none */
extern s32 menu_save_view_frame_x_table[9];     /* view frame x (first view) */
extern s32 menu_save_view_frame_y_table[9];     /* view frame y */
extern s32 menu_file_info_play_time_x_table[9];     /* play time: x of the two separators and seven digits */
extern s32 menu_save_view_level_x;         /* view level digits x */
extern s32 menu_save_view_level_y;         /* view level digits y */
extern s32 menu_save_title_x;         /* save title x, y */
extern s32 menu_save_title_y;
extern s32 menu_save_view_hp_x;         /* view HP digits x, y */
extern s32 menu_save_view_hp_y;
extern s32 menu_save_view_max_hp_x;         /* view maximum HP digits x, y */
extern s32 menu_save_view_max_hp_y;
extern s32 menu_save_view_ep_x;         /* view EP digits x, y */
extern s32 menu_save_view_ep_y;
extern s32 menu_save_view_max_ep_x;         /* view maximum EP digits x, y */
extern s32 menu_save_view_max_ep_y;
extern s32 menu_card_blocks_used[2]; /* per port: blocks the listed files use (15 fill a card) */

/* The card and file screen functions that another unit calls, or its own
 * before defining them. */
void menu_cd_ask_for_disc(u8 disc);
void menu_card_poll_ports(void);
void menu_card_list_unscanned_ports(void);
s32 menu_notice_ask_yes_no(u8 message, u8 confirm, u8 arg);
void menu_card_restart_access(void);
void menu_file_screen_leave_card_mode(void);
u8 menu_file_screen_run(u8 mode, u8 save);
void menu_copy_game_data_to_save(SaveData *save);
void menu_restore_game_data_from_save(SaveData *save, MenuTables *tables);
void menu_file_slots_alloc(void);
void menu_file_slots_free(void);
void menu_file_info_alloc(void);
void menu_file_info_free(void);
void menu_file_info_clear(void);
void menu_file_info_show(s32 index, u8 rebuild);
void menu_save_icon_upload(s32 file);
void menu_cd_stop_drive(void);
s32 menu_cd_check_disc(s32 disc);

#endif
