#ifndef SLOT39_FILE_H
#define SLOT39_FILE_H

#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "resident/menu.h"
#include "menu/tables.h"

/* The menu overlay's memory card and file screen (slot39): the card scan
 * and messages, save and load (the save file layout and the game data it
 * copies), the file screen's slots and views of a save, and the disc checks. */

/* The second half of a listed file's header block (+100). */
typedef struct MenuSaveName {
    u8 text[0x14]; /* two-byte text */
} MenuSaveName;

typedef struct MenuSaveInfo {
    u8 pad0[0x24];
    MenuSaveName names[4]; /* 24: names of the sheet entries */
} MenuSaveInfo;

/* The record 801e76ec passes to 801e6ae8. */
typedef struct MenuViewSet {
    s32 time; /* 0: play time in frames */
    u16 valueA[3]; /* 4: per view */
    u16 valueB[3]; /* A: per view */
    u8 valueC[3]; /* 10: per view */
    u8 valueD[3]; /* 13: per view */
    u8 levels[3]; /* 16: per view: number shown in the first digit row */
    u8 unk19[3]; /* 19: per view: number of the second digit row */
    u8 images[4]; /* 1C: sheet image per view (+14e), ff none */
    u8 pad20[0x3];
    u8 unk23; /* 23: shown plus one as two digits */
} MenuViewSet;

/* A save information view (801e76ec). */
typedef struct MenuView {
    POLY_FT4 image[2]; /* 0: the entry's sheet image (801e6ae8) */
    POLY_FT4 frame[9][2]; /* 50: frame sprite list */
    POLY_FT4 levelDigits[6][2]; /* 320: digit sprite list of the set's level (+16), per buffer */
    POLY_FT4 aDigits[3][2]; /* 500: of value A (+4) */
    POLY_FT4 bDigits[3][2]; /* 5F0: of value B (+a) */
    POLY_FT4 cDigits[2][2]; /* 6E0: of value C (+10) */
    POLY_FT4 dDigits[2][2]; /* 780: of value D (+13) */
    POLY_FT4 name[2]; /* 820: the entry's name image */
    u8 levelCount; /* 870 */
    u8 unk871; /* 871 */
    u8 aCount; /* 872 */
    u8 bCount; /* 873 */
    u8 cCount; /* 874 */
    u8 dCount; /* 875 */
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

/* The disc label read from sector 0 of the data track (file 17). */
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
    s32 time; /* 0: play time in frames */
    u8 pad4[0x20];
    SaveWordsDC names; /* 24: game data 0 */
    SaveWords190 unk100; /* 100: game data dc */
    SaveWordsA4 chars[11]; /* 290: character records */
    SaveGear gears[20]; /* 99C */
    SaveWords78 unkE4C; /* E4C: game data 1648 */
    SaveWords160 records; /* EC4: game data 16c0 */
    SaveWords100 unk1024; /* 1024: game data 1820 */
    u8 unk1124[0xA38]; /* 1124: game data 1920 */
} SaveData;

/* libapi directory entry (firstfile/nextfile). */
struct DIRENTRY {
    char name[20];
    s32 attr;
    s32 size;
    struct DIRENTRY *next;
    s32 head;
    char system[4];
};

/* The summary at the start of a save payload (801cba4c). */
typedef struct MenuSavePayload {
    s32 time; /* 0: play time */
    u16 hp[3]; /* 4: per party slot */
    u16 hpMax[3]; /* A */
    u8 ep[3]; /* 10 */
    u8 epMax[3]; /* 13 */
    u8 unk16[3]; /* 16 */
    u8 unk19[3]; /* 19 */
    u8 ids[3]; /* 1C: character, ff empty */
    u8 unk1F; /* 1F */
    u8 pad20[0x3];
    u8 digit; /* 23: file digit */
} MenuSavePayload;

/* The 31 names at the start of the game data (encoded in the save). */
#define GAME_NAMES ((u8 *)&D_8006D634)

/* The battle script variables (a resident common): the save keeps them
 * in its flag words. No shared header declares them; battle reads them
 * signed. */
extern u16 D_8005A3A0[16];
extern u8 D_801EA8FC;    /* the last choice was cancelled */
extern u16 D_801EA610[96]; /* two-byte codes of the ASCII characters 0x20-0x7F */
extern s16 D_801E9894[32][2];    /* image block x */
extern s16 D_801E9914[32][2];    /* image block y */
extern s32 D_801EA004[];
extern s32 D_801EA010[];
extern s32 D_801E9994[21];    /* text character x per column */
extern s32 D_801E99E8[];      /* text character y per row */
extern s32 D_801E99F0;        /* cursor sprite x */
extern s32 D_801E99F8;        /* cursor sprite y */
extern s32 D_801EA494[18];    /* view frame images, ffff none */
extern s32 D_801E9F98[9];     /* view frame x (first view) */
extern s32 D_801E9FBC[9];     /* view frame y */
extern s32 D_801E9FE0[9];     /* play time: x of the two separators and seven digits */
extern s32 D_801EA01C;         /* view digit row x */
extern s32 D_801EA020;         /* view digit row y */
extern s32 D_801EA04C;         /* save title x, y */
extern s32 D_801EA050;
extern s32 D_801EA02C;         /* value A digits x, y */
extern s32 D_801EA030;
extern s32 D_801EA034;         /* value B digits x, y */
extern s32 D_801EA038;
extern s32 D_801EA03C;         /* value C digits x, y */
extern s32 D_801EA040;
extern s32 D_801EA044;         /* value D digits x, y */
extern s32 D_801EA048;
extern s32 D_801EA900[2]; /* per port: blocks the listed files use (15 fill a card) */

void func_801C57A4(void);
void func_801C58EC(void);
void func_801C6400(void);
void func_801C8694(u8 arg0);
u8 func_801C881C(void);
s32 func_801C891C(s32 channel);
u8 func_801C8A10(u8 port);
void func_801C8BEC(void);
void func_801C8CA4(u8 port);
u8 func_801C8D78(u8 port);
void func_801C8EE8(void);
void func_801C9270(s32 port);
u8 func_801C93A8(void);
s32 func_801C9BCC(s32 mode);
s32 func_801C9D34(s32 mode);
void func_801C9EF4(s32 mode, s32 slot);
void func_801CA1D4(s32 mode, s32 slot);
void func_801CA480(s32 mode, s32 slot);
void func_801CA5F0(s32 mode, s32 slot);
s32 func_801CA750(s32 mode);
u8 func_801CAA38(u8 arg);
s32 func_801CACF8(u8 message, u8 confirm, u8 arg);
void func_801CADB0(void);
void func_801CAE08(u8 mode);
void func_801CB184(void);
void func_801CB28C(s32 *save);
u8 func_801CB304(void);
void func_801CBA4C(MenuSavePayload *payload, u8 port, u8 digit);
u8 func_801CBD90(u8 arg);
u8 func_801CC6D8(void);
u8 func_801CD2AC(void);
void func_801CF37C(void);
void func_801CF5E4(s32 first, s32 firstSlot, s32 end);
void func_801CF8D8(void);
void func_801CFB48(void);
void func_801CFF64(void);
void func_801D01D0(void);
void func_801D02D8(void);
void func_801D9B08(void);
void func_801D9E3C(void);
u8 func_801D9F98(u8 mode, u8 save);
void func_801E4A28(SaveData *save);
void func_801E4D10(SaveData *save, MenuTables *tables);
void func_801E56E8(s32 index);
void func_801E5924(s32 index);
void func_801E5ACC(void);
void func_801E5B3C(void);
void func_801E5B88(void);
void func_801E5E4C(void);
void func_801E61B0(void);
void func_801E6450(void);
void func_801E649C(void);
void func_801E64E0(void);
void func_801E6668(s32 index);
void func_801E68AC(MenuViewSet *set);
void func_801E6AE8(u8 index, MenuViewSet *set);
void func_801E6B70(u8 index, MenuViewSet *set);
void func_801E6CFC(u8 index, MenuViewSet *set);
void func_801E6F5C(u8 index, MenuViewSet *set);
void func_801E71B4(u8 index, MenuViewSet *set, s32 file);
void func_801E733C(void);
void func_801E76EC(s32 index);
void func_801E781C(s32 index, u8 rebuild);
void func_801E78C8(s32 file);
void func_801E92CC(void);
void func_801E9340(char *name, void *buffer, s32 size);
s32 func_801E93A0(s32 disc);

#endif
