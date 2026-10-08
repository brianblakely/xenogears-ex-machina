/*
 * Menu overlay (mode 5; Disc 1 slot 39 / unpacked 2597, Disc 2 slot 34 /
 * unpacked 2592; loaded at 801c5000). The resident mode table enters it
 * through 8001c634 with no overlay preloaded. By the menu kind in
 * D_80059460 it runs the field main menu (items, equipment and the other
 * commands; kind 0), the title screen's file (memory-card load) screen
 * (kind 2) or kind 6. It keeps its state behind D_800625A0 and reads and
 * writes "bu00:"/"bu10:" memory-card files (BASLUS-00664...).
 */
#include "menu.h"

/* Declared here only: slot39_801DBE54.c calls it without a prototype. */
void func_801D7CFC(u8 slot, u8 mode, u8 arg2);

/* The overlay's initialized data: all of it is defined here, ahead of the
 * units' uninitialized variables. */
u8 D_801E96A4 = 0; /* the file screen saves (nonzero) or loads */
u8 D_801E96A5 = 0;
u8 D_801E96A6 = 8; /* unreferenced */
/* single-bit masks */
u16 D_801E96A8[16] = {
    0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
    0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000, 0x8000,
};
u16 D_801E96C8[16] = {
    0x8000, 0x4000, 0x2000, 0x1000, 0x0800, 0x0400, 0x0200, 0x0100,
    0x0080, 0x0040, 0x0020, 0x0010, 0x0008, 0x0004, 0x0002, 0x0001,
};
u32 D_801E96E8[32] = {
    0x00000001, 0x00000002, 0x00000004, 0x00000008, 0x00000010, 0x00000020, 0x00000040, 0x00000080,
    0x00000100, 0x00000200, 0x00000400, 0x00000800, 0x00001000, 0x00002000, 0x00004000, 0x00008000,
    0x00010000, 0x00020000, 0x00040000, 0x00080000, 0x00100000, 0x00200000, 0x00400000, 0x00800000,
    0x01000000, 0x02000000, 0x04000000, 0x08000000, 0x10000000, 0x20000000, 0x40000000, 0x80000000,
};
s32 D_801E9768[4] = { 0, -3, -1, -2 };
u8 D_801E9778 = 0; /* a card changed during a choice */
u8 D_801E9779 = 0x1E; /* frames between card checks */
u8 D_801E977A = 1; /* inside a command */
s32 D_801E977C[2] = { 0x159, 0x15A }; /* detail panel tab sprites */
u8 D_801E9784 = 1; /* nonzero checks the reset combination */
u8 D_801E9785 = 0; /* the target panels are allocated */
s32 D_801E9788[3] = { 0x11A, 0x11A, 0xA2 }; /* arts screen window sizes per kind */
s32 D_801E9794[3] = { 0xDA, 0xDA, 0xC8 };
s32 D_801E97A0[3] = { 0x52, 0x52, 0x64 };
/* 801E1544 screen: five sheet images per row (ff none) for 13 rows; the last three bytes are never read */
u8 D_801E97AC[68] = {
    0xFA, 0xF9, 0xFF, 0xFF, 0xFF,
    0xFA, 0xFA, 0xF9, 0xFF, 0xFF,
    0xFB, 0xF9, 0xFF, 0xFF, 0xFF,
    0xFA, 0xFA, 0xFA, 0xF9, 0xFF,
    0xFA, 0xFB, 0xF9, 0xFF, 0xFF,
    0xFB, 0xFA, 0xF9, 0xFF, 0xFF,
    0xF9, 0xF9, 0xFF, 0xFF, 0xFF,
    0xFA, 0xFA, 0xFA, 0xFA, 0xF9,
    0xFA, 0xFA, 0xFB, 0xF9, 0xFF,
    0xFA, 0xFB, 0xFA, 0xF9, 0xFF,
    0xFB, 0xFA, 0xFA, 0xF9, 0xFF,
    0xFB, 0xFB, 0xF9, 0xFF, 0xFF,
    0xF9, 0xFA, 0xF9, 0xFF, 0xFF,
    0x00, 0x07, 0x2E,
};
u16 D_801E97F0[12] = { 2, 0, 1, 0, 6, 0, 0, 1, 0, 1, 0, 0 };
u8 D_801E9808[20] = { 0, 0, 1, 2, 3, 4, 5, 7, 8, 6, 1, 9, 3, 4, 5, 0, 9, 0xF, 0xF, 0xF }; /* pilot character of each gear */
/* card slot (port * 16 + n) of each cursor position */
s32 D_801E981C[30] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E,
};
/* image block x */
s16 D_801E9894[32][2] = {
    { 0x20, 0 }, { 0x28, 0 }, { 0x30, 0 }, { 0x38, 0 }, { 0x40, 0 }, { 0x48, 0 }, { 0x50, 0 }, { 0x58, 0 },
    { 0x60, 0 }, { 0x68, 0 }, { 0x70, 0 }, { 0x78, 0 }, { 0x80, 0 }, { 0x88, 0 }, { 0x90, 0 }, { 0x140, 0 },
    { 0xB0, 0 }, { 0xB8, 0 }, { 0xC0, 0 }, { 0xC8, 0 }, { 0xD0, 0 }, { 0xD8, 0 }, { 0xE0, 0 }, { 0xE8, 0 },
    { 0xF0, 0 }, { 0xF8, 0 }, { 0x100, 0 }, { 0x108, 0 }, { 0x110, 0 }, { 0x118, 0 }, { 0x120, 0 }, { 0x140, 0 },
};
/* image block y */
s16 D_801E9914[32][2] = {
    { 0xE, 0 }, { 0x22, 0 }, { 0x36, 0 }, { 0xE, 0 }, { 0x22, 0 }, { 0x36, 0 }, { 0xE, 0 }, { 0x22, 0 },
    { 0x36, 0 }, { 0xE, 0 }, { 0x22, 0 }, { 0x36, 0 }, { 0xE, 0 }, { 0x22, 0 }, { 0x36, 0 }, { 0x100, 0 },
    { 0xE, 0 }, { 0x22, 0 }, { 0x36, 0 }, { 0xE, 0 }, { 0x22, 0 }, { 0x36, 0 }, { 0xE, 0 }, { 0x22, 0 },
    { 0x36, 0 }, { 0xE, 0 }, { 0x22, 0 }, { 0x36, 0 }, { 0xE, 0 }, { 0x22, 0 }, { 0x36, 0 }, { 0x100, 0 },
};
/* text character x per column */
s32 D_801E9994[21] = {
    0x30, 0x3C, 0x48, 0x54, 0x60, 0x6C, 0x78,
    0x84, 0x90, 0x9C, 0xA8, 0xB4, 0xC0, 0xCC,
    0xD8, 0xE4, 0xF0, 0xFC, 0x108, 0x114, 0x120,
};
s32 D_801E99E8[2] = { 0x56, 0x66 }; /* text character y per row */
s32 D_801E99F0 = 0x10; /* cursor sprite x */
s32 D_801E99F4 = 0x30; /* unreferenced */
s32 D_801E99F8 = 0x56; /* cursor sprite y */
s32 D_801E99FC = 0x76; /* unreferenced */
s32 D_801E9A00[7] = { 0x49, 0x48, 0x43, 0x3C, 0x34, 0x26, 0x17 }; /* highlight positions: x */
s32 D_801E9A1C[4] = { 0x25, 0x24, 0x1F, 0x18 }; /* choice highlight positions: x */
s32 D_801E9A2C[7] = { 0xC9, 0xB5, 0xA2, 0x90, 0x7E, 0x6F, 0x61 }; /* highlight positions: y */
s32 D_801E9A48[4] = { 0xC9, 0xB5, 0xA2, 0x90 }; /* choice highlight positions: y */
s32 D_801E9A58[4] = { 0, 0, 0xB4, 0x114 }; /* marker positions */
s32 D_801E9A68[4] = { 0, 0, 0xC8, 0xC8 };
s32 D_801E9A78[20] = {
    0x48, 0x30, 0x38, 0x58, 0x30, 0x38, 0x58, 0x30, 0x38, 0x58,
    0x80, 0x88, 0x90, 0x98, 0x98, 0xA8, 0xB0, 0xB0, 0x80, 0x80,
};
s32 D_801E9AC8[20] = {
    0, 8, 8, 8, 0x20, 0x20, 0x20, 0x28, 0x28, 0x28,
    0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x20, 0x28,
};
/* Field block number offsets (x, y). */
s32 D_801E9B18 = 0x40; /* +62 */
s32 D_801E9B1C = 8;
s32 D_801E9B20 = 0x60; /* +63 */
s32 D_801E9B24 = 8;
s32 D_801E9B28 = 0x40; /* hp */
s32 D_801E9B2C = 0x20;
s32 D_801E9B30 = 0x60; /* hp max */
s32 D_801E9B34 = 0x20;
s32 D_801E9B38 = 0x48; /* ep */
s32 D_801E9B3C = 0x28;
s32 D_801E9B40 = 0x60; /* ep max */
s32 D_801E9B44 = 0x28;
s32 D_801E9B48 = 0x88; /* exp */
s32 D_801E9B4C = 0x20;
s32 D_801E9B50 = 0x88; /* exp to next level */
s32 D_801E9B54 = 0x28;
s32 D_801E9B58 = 0x30; /* field block portrait offset: x */
s32 D_801E9B5C = 0x11; /* y */
/* detail panel part positions, 24 per layout: x */
s32 D_801E9B60[48] = {
    0x50, 0x58, 0x70, 0x78, 0x80, 0xB0, 0xB8, 0xC0, 0xC8, 0xC8, 0xD8, 0xE0,
    0xE0, 0x48, 0x50, 0x70, 0x48, 0x50, 0x70, 0x38, 0x68, 0x80, 0x48, 0x48,
    0x38, 0x40, 0x58, 0x60, 0x68, 0x98, 0xA0, 0xA8, 0xB0, 0xB0, 0xC0, 0xC8,
    0xC8, 0x30, 0x38, 0x38, 0x30, 0x38, 0x38, 0x28, 0x38, 0x38, 0x30, 0x30,
};
/* y */
s32 D_801E9C20[48] = {
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 0x2E, 0x2E, 0x2E, 0x36, 0x36, 0x36, 0x46, 0x46, 0x46, 0xE, 0x16,
    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
    6, 0x2E, 0x2E, 0x36, 0x3E, 0x3E, 0x46, 0x4E, 0x36, 0x36, 0xE, 0x16,
};
/* Detail panel number positions (x, y). */
s32 D_801E9CE0 = 0x50; /* level */
s32 D_801E9CE4 = 0xE;
s32 D_801E9CE8 = 0x50; /* +63 */
s32 D_801E9CEC = 0x16;
s32 D_801E9CF0 = 0x58; /* hp */
s32 D_801E9CF4 = 0x2E;
s32 D_801E9CF8 = 0x78; /* hp max */
s32 D_801E9CFC = 0x2E;
s32 D_801E9D00 = 0x60; /* ep */
s32 D_801E9D04 = 0x36;
s32 D_801E9D08 = 0x78; /* ep max */
s32 D_801E9D0C = 0x36;
s32 D_801E9D10 = 0x70; /* +3c */
s32 D_801E9D14 = 0xE;
s32 D_801E9D18 = 0x70; /* +40 */
s32 D_801E9D1C = 0x16;
s32 D_801E9D20 = 0xB8; /* exp */
s32 D_801E9D24 = 0xE;
s32 D_801E9D28 = 0xB8; /* exp to next level */
s32 D_801E9D2C = 0x16;
s32 D_801E9D30 = 0x50; /* +77..79 value */
s32 D_801E9D34 = 0x46;
s32 D_801E9D38 = 0x48; /* detail panel portrait position: x */
s32 D_801E9D3C = 0x20; /* y */
s32 D_801E9D40[7] = { 8, 8, 8, 8, 8, 8, 8 }; /* stat name positions per row: x */
s32 D_801E9D5C[7] = { 0, 8, 0x10, 0x18, 0x20, 0x28, 0x30 }; /* y */
s32 D_801E9D78 = 0x28; /* stat bar x offset */
s32 D_801E9D7C = 2; /* stat bar y offset */
s32 D_801E9D80 = 0x68; /* stat digit x offset */
s32 D_801E9D84 = 0; /* stat digit y offset */
s32 D_801E9D88[13] = {
    0x76, 0x85, 0x92, 0x9F, 0xAE, 0x1E, 0x39, 0x46, 0x53, 0x1E, 0x39, 0x53, 0x6C,
};
s32 D_801E9DBC[8] = { 0x15, 0x31, 0x3D, 0x4B, 0x15, 0x31, 0x4B, 0x64 }; /* equipment screen: part cursor y by special * 4 + part */
/* arts list cost x positions */
s32 D_801E9DDC[14] = {
    0x8C, 0x114, 0x8C, 0x114, 0x8C, 0x114, 0x8C, 0x114, 0x8C, 0x114, 0x8C, 0x114, 0xFC, 0x114,
};
/* arts list cost y positions */
s32 D_801E9E14[14] = {
    0x12, 0x12, 0x22, 0x22, 0x32, 0x32, 0x42, 0x42, 0x52, 0x52, 0x62, 0x62, 0xAC, 0xAC,
};
s32 D_801E9E4C[3] = { 0x98, 0x70, 0xA8 }; /* party label positions: x */
s32 D_801E9E58[3] = { 0x76, 0x92, 0xAE }; /* y */
s32 D_801E9E64[8] = { 0x12, 6, 0x12, 6, 6, 0x12, 0, 0 }; /* label x offsets: field menu commands */
s32 D_801E9E84[4] = { 0, 6, 0, 0 }; /* title file screen commands */
s32 D_801E9E94[3] = { 0xC, 6, 0xC }; /* choice label x offsets */
s32 D_801E9EA0[9] = { 0x12, 0xC, 0xC, 0xB0, 0x80, 0xB0, 0x76, 0x92, 0xAE };
s32 D_801E9EC4[8] = { 0x98, 0x98, 0x98, 0xBC, 0xBC, 0xBC, 0, 0 }; /* label x (mode 1) */
u16 D_801E9EE4 = 0x93; /* label y (mode 1) */
/* label x (modes 2, 5 from 8) */
s32 D_801E9EE8[16] = {
    0xD2, 0xD2, 0xD2, 0xF6, 0xF6, 0xF6, 0xE4, 0x10C,
    0xD2, 0xD2, 0xD2, 0xF6, 0xF6, 0xF6, 0xD4, 0x10C,
};
s32 D_801E9F28[2] = { 0x8C, 0xAC }; /* label y per row (mode 2) */
s32 D_801E9F30[6] = { 0x12, 0x2C, 0x12, 0x2C, 0x46, 0x60 }; /* label y per row (mode 3) */
s32 D_801E9F48[8] = { 0xC, 6, 6, 0x12, 0, 0, 0xC, 6 }; /* status command label x offsets (page 0 and 6) */
s32 D_801E9F68[2] = { 6, 0xC }; /* label x (mode 6) */
s32 D_801E9F70[6] = { 0, 0, 0x98, 0x70, 0x76, 0x92 }; /* label y (mode 6) */
s32 D_801E9F88[4] = { 6, 0xC, 6, 0 }; /* sound mode label x offsets */
s32 D_801E9F98[9] = { 0x4C, 0x54, 0x74, 0x4C, 0x54, 0x74, 0x4C, 0x54, 0x74 }; /* view frame x (first view) */
s32 D_801E9FBC[9] = { 0x4E, 0x4E, 0x4E, 0x62, 0x62, 0x62, 0x6A, 0x6A, 0x6A }; /* view frame y */
s32 D_801E9FE0[9] = { 0x34, 0x4C, 0x1C, 0x24, 0x2C, 0x3C, 0x44, 0x54, 0x5C }; /* play time: x of the two separators and seven digits */
s32 D_801EA004[3] = { 0xC, 0x1C, 0x2C };
s32 D_801EA010[3] = { 0x4C, 0x54, 0x5C };
s32 D_801EA01C = 0x5C; /* view digit row x */
s32 D_801EA020 = 0x4E; /* view digit row y */
s32 D_801EA024 = 0x7C; /* unreferenced */
s32 D_801EA028 = 0x4E; /* unreferenced */
s32 D_801EA02C = 0x5C; /* value A digits x, y */
s32 D_801EA030 = 0x62;
s32 D_801EA034 = 0x7C; /* value B digits x, y */
s32 D_801EA038 = 0x62;
s32 D_801EA03C = 0x64; /* value C digits x, y */
s32 D_801EA040 = 0x6A;
s32 D_801EA044 = 0x7C; /* value D digits x, y */
s32 D_801EA048 = 0x6A;
s32 D_801EA04C = 0x70; /* save title x, y */
s32 D_801EA050 = 0x74;
/* Target panel layouts: x and y anchors. */
MenuAnchor D_801EA054[1] = {
    { 0x48, 0x50, 0x70, 0x48, 0x50, 0x70, 0x48, 0x50, 0x70, 0x18, 0x58, 0x78, 0x58, 0x78, 0x60, 0x78, 0x48 },
};
MenuAnchor D_801EA098[1] = {
    { 0x48, 0x50, 0x70, 0x48, 0x50, 0x58, 0x50, 0x50, 0x50, 0x18, 0x58, 0x78, 0x60, 0x60, 0x60, 0x78, 0x48 },
};
MenuAnchor D_801EA0DC[1] = {
    { 0x1E, 0x1E, 0x1E, 0x36, 0x36, 0x36, 0x3E, 0x3E, 0x3E, 0x16, 0x1E, 0x1E, 0x36, 0x36, 0x3E, 0x3E, 0x28 },
};
MenuAnchor D_801EA120[1] = {
    { 0x1E, 0x1E, 0x1E, 0x36, 0x36, 0x3E, 0x36, 0x36, 0x36, 0x16, 0x1E, 0x1E, 0x36, 0x3E, 0x3E, 0x3E, 0x28 },
};
s32 D_801EA164[2] = { 0, 0x114 }; /* party window sprite x */
s32 D_801EA16C[4] = { 0x66, 0x76, 0x76, 0xC }; /* party window sprite y per row */
s32 D_801EA17C[4] = { 0x34, 0xA0, 0x24, 0xA0 }; /* portrait panel x per mode */
s32 D_801EA18C[4] = { 0x80, 0xC2, 0x20, 0x5A }; /* portrait panel y per mode */
/* field menu command cursor images */
MenuCommandImages D_801EA19C[7] = {
    { 0x109, 0x131 }, { 0x10A, 0x130 }, { 0x10B, 0x12F }, { 0x10C, 0x12E },
    { 0x10D, 0x12D }, { 0x10E, 0x12C }, { 0x10F, 0x12B },
};
/* title file screen cursor images */
MenuCommandImages D_801EA1D4[3] = {
    { 0x109, 0x125 }, { 0x10A, 0x124 }, { 0x10B, 0x123 },
};
/* per command: four choices of cursor and label images (ffff none) */
s32 D_801EA1EC[88] = {
    0x110, 0x142, 0x111, 0x141, 0x112, 0x140, 0xFFFF, 0xFFFF,
    0x110, 0x12A, 0x111, 0x129, 0x112, 0x127, 0xFFFF, 0xFFFF,
    0x110, 0x13E, 0x111, 0x13D, 0x112, 0x13C, 0x113, 0x13F,
    0x110, 0x126, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0x110, 0x126, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0x110, 0x126, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0x110, 0x134, 0x111, 0x133, 0x112, 0x132, 0xFFFF, 0xFFFF,
    0x110, 0x131, 0x111, 0x126, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0x110, 0x12A, 0x111, 0x129, 0x112, 0x128, 0xFFFF, 0xFFFF,
    0x110, 0x126, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    0x110, 0x12A, 0x111, 0x129, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
};
/* field block part images, ffff none */
s32 D_801EA34C[20] = {
    0xFFFF, 0x15, 0x1F, 0xFFFF, 0x11, 0x19, 0x3E, 0xE, 0x19, 0x3E,
    0x17, 0x28, 0x3B, 0x37, 0x37, 0x15, 0x1F, 0x1F, 0xFFFF, 0xFFFF,
};
/* detail panel part sprites, 24 per layout, ffff none */
s32 D_801EA39C[48] = {
    0x15, 0x1F, 0xE, 0x3B, 0x33, 0x17, 0x28, 0x3B, 0x37, 0x37, 0x15, 0x1F,
    0x1F, 0x11, 0x19, 0x3E, 0xE, 0x19, 0x3E, 0xE6, 0x91, 0xE7, 0xFFFF, 0xFFFF,
    0x15, 0x1F, 0xE, 0x3B, 0x33, 0x17, 0x28, 0x3B, 0x37, 0x37, 0x15, 0x1F,
    0x1F, 0x11, 0x19, 0x3E, 0xF, 0x15, 0x3E, 0xE6, 0x3E, 0x3E, 0xFFFF, 0xFFFF,
};
/* stat name sprites, seven per start row */
s32 D_801EA45C[14] = {
    0xDE, 0xDF, 0xE0, 0xE1, 0xE2, 0xE4, 0xEB,
    0xDE, 0xF3, 0xF4, 0xF6, 0xEB, 0xF7, 0xF7,
};
/* view frame images (ffff none) */
s32 D_801EA494[18] = {
    0x15, 0x1F, 0xFFFF, 0x11, 0x19, 0x3E, 0xE, 0x19, 0x3E,
    0, 0, 0, 0, 0, 0, 0, 0, 0,
};
/* status panel layout sprites, nine per layout, ffff none */
s32 D_801EA4DC[18] = {
    0x15, 0x1F, 0xFFFF, 0x11, 0x19, 0x3E, 0xE, 0x19, 0x3E,
    0x15, 0x1F, 0xFFFF, 0x11, 0x19, 0x3E, 0x19, 0x19, 0x19,
};
/* Label lists. */
u8 D_801EA524[4] = { 0x09, 0x0A, 0x0B, 0x0C }; /* label image layout */
u8 D_801EA528[8] = { 0x0A, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x02 }; /* field menu command labels */
u8 D_801EA530[2] = { 0x08, 0x01 };
u8 D_801EA534[6] = { 0x12, 0x11, 0x10, 0x13, 0x14, 0x15 }; /* party label layout */
/* file screen command labels: save, then load */
u8 D_801EA53C[12] = {
    0x1B, 0x1A, 0x19, 0x18, 0x16, 0x17,
    0x1B, 0x1A, 0x18, 0x19, 0x16, 0x17,
};
u8 D_801EA548[8] = { 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F }; /* save/load screen labels */
u8 D_801EA550[8] = { 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x7A, 0x7B }; /* item target labels */
u8 D_801EA558[12] = {
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75,
    0x70, 0x71, 0x72, 0x7B, 0x7B, 0x75,
};
u8 D_801EA564[2] = { 0x76, 0x76 }; /* 801E1014 screen labels */
/* status screen labels: a character's page, then a gear's */
u8 D_801EA568[12] = {
    0x12, 0x11, 0x77, 0x78, 0x13, 0x14,
    0x12, 0x11, 0x77, 0x79, 0x13, 0x14,
};
u8 D_801EA574[4] = { 0x9B, 0x9C, 0x9D, 0x7B }; /* sound mode labels */
/* Label image positions in the sheet, per row pair: x / 4, then y. */
s32 D_801EA578[19] = {
    0x18, 0, 0x18, 0, 0x18, 0, 0x18, 0, 0x18, 0, 0x18, 0, 0x18, 0, 0x18, 0, 0x18, 0, 0x18,
};
s32 D_801EA5C4[19] = {
    0x3B, 0x48, 0x48, 0x55, 0x55, 0x62, 0x62, 0x6F, 0x6F, 0x7C, 0x7C, 0x89, 0x89, 0x96, 0x96, 0xA3, 0xA3, 0xB0, 0xB0,
};
/* Two-byte (Shift JIS) codes of the ASCII characters 0x20-0x7F. */
u16 D_801EA610[96] = {
    0x8140, 0x8149, 0x814A, 0x8194, 0x8190, 0x8193, 0x8195, 0x8166,
    0x8169, 0x816A, 0x8196, 0x817B, 0x8143, 0x817C, 0x8144, 0x815E,
    0x824F, 0x8250, 0x8251, 0x8252, 0x8253, 0x8254, 0x8255, 0x8256,
    0x8257, 0x8258, 0x8146, 0x8147, 0x8183, 0x8181, 0x8184, 0x8148,
    0x8197, 0x8260, 0x8261, 0x8262, 0x8263, 0x8264, 0x8265, 0x8266,
    0x8267, 0x8268, 0x8269, 0x826A, 0x826B, 0x826C, 0x826D, 0x826E,
    0x826F, 0x8270, 0x8271, 0x8272, 0x8273, 0x8274, 0x8275, 0x8276,
    0x8277, 0x8278, 0x8279, 0x816D, 0x818F, 0x816E, 0x81F4, 0x8151,
    0x8165, 0x8281, 0x8282, 0x8283, 0x8284, 0x8285, 0x8286, 0x8287,
    0x8288, 0x8289, 0x828A, 0x828B, 0x828C, 0x828D, 0x828E, 0x828F,
    0x8290, 0x8291, 0x8292, 0x8293, 0x8294, 0x8295, 0x8296, 0x8297,
    0x8298, 0x8299, 0x829A, 0x816F, 0x8162, 0x8170, 0x814D, 0x8140,
};

/* The unit's variables, zero in the image after the initialized data. The
 * original allocated them in 4-byte slots; from D_801EA714 on (two bytes
 * four apart) that layout is not a C one and stays generated data. */
u8 D_801EA6D0[2][16] = { 0 }; /* per port and save slot: a save of this game exists */
s32 D_801EA6F0 = 0;           /* unreferenced */
u8 *D_801EA6F4 = NULL;        /* the save information of the last matched file */
u8 D_801EA6F8 = 0;
s32 D_801EA6FC = 0;           /* gauge: from, to, difference and lengths */
s32 D_801EA700 = 0;
s32 D_801EA704 = 0;
s32 D_801EA708 = 0;
s32 D_801EA70C = 0;
u8 D_801EA710 = 0;

/* Run the command at `offset` past the top cursor (0 back, 1 load/save file,
 * 2..6 the field-menu screens, 7/8 the title file screen's load and new game,
 * 9 reset), then restore the command window. Returns 0 when the menu ends. */
u8 func_801C531C(u8 offset) {
    u8 redraw;
    u8 stay;

    D_801E977A = 1;
    stay = 1;
    redraw = 1;
    switch (D_800625A0->cursor + offset) {
    case 7:
        redraw = func_801D9808();
        break;
    case 8:
        if (func_801D9F98(1, 0)) {
            D_800594D0 = 2;
            stay = 0;
        }
        break;
    case 1:
        redraw = func_801D9F98(0, D_801E96A4);
        break;
    case 2:
        redraw = func_801E23CC();
        break;
    case 3:
        redraw = func_801DE29C(D_800625A0->firstMember, 1);
        break;
    case 4:
        redraw = func_801DBE54();
        break;
    case 5:
        redraw = func_801E0F78(D_800625A0->firstMember, 1);
        break;
    case 6:
        redraw = func_801E2BE4();
        break;
    case 9:
        func_8001B970();
    case 0:
        redraw = 0;
        stay = 0;
        break;
    }
    D_800625A0->card->mode = 0;
    if (redraw) {
        func_801D1EB0();
        if (D_80059460 == 0) {
            func_801D29A8(1, 0);
        } else if (D_80059460 == 2) {
            func_801E8018(8, D_800625A0->commandLabels, D_801EA530, D_800625A0->party->labels);
            D_800625A0->primitives->shade = 0x4c;
        }
    }
    func_801E3088(offset);
    func_801D3674();
    D_800625A0->screenImages->captured = 0;
    D_800625A0->screenImages->refresh = 1;
    D_800625A0->party->redraw4 = 1;
    D_800625A0->party->redraw3 = 1;
    D_800625A0->cursorShown = 0xff;
    D_800625A0->party->redrawA = 0;
    D_801E9784 = 1;
    return stay;
}

/* The field menu's command loop: move the cursor over the seven commands and
 * run the chosen one until the menu is left. */
void func_801C55A0(void) {
    u8 ok;
    u8 stay;

    stay = 1;
    do {
        func_801C7BF4();
        switch (D_800625A0->input) {
        case 4:
            ok = 1;
            if (D_800625A0->cursor == 2 && D_800625A0->fighters == 0) {
                ok = 0;
                func_801C8574(4);
            }
            if (ok) {
                D_800625A0->screenImages->captured = 1;
                func_801D22C4();
                func_801E8044(8, D_800625A0->party->labels);
                stay = func_801C531C(0);
            }
            break;
        case 5:
            stay = 0;
            break;
        case 1:
            if (D_800625A0->cursor != 0) {
                D_800625A0->cursor--;
            } else {
                D_800625A0->cursor = 6;
            }
            break;
        case 3:
            if (++D_800625A0->cursor >= 7) {
                D_800625A0->cursor = 0;
            }
            break;
        }
        if (D_800625A0->cursor != D_800625A0->cursorShown) {
            func_801E8978(7, D_800625A0->cursor, D_801EA19C);
            func_801E8070(8, D_800625A0->commandLabels, D_801EA528, D_801E9E64, D_800625A0->party->labels,
                          D_800625A0->cursor, 0, 0);
            D_800625A0->cursorShown = D_800625A0->cursor;
        }
    } while (stay);
}

/* Menu kind 6: wait for the pad to be released, check the cards and, when a
 * file can be saved, run the save file screen; then the loaded-disc check. */
void func_801C57A4(void) {
    u8 save;
    u8 found;

    D_80059171 = 1;
    D_801E977A = 0;
    save = 1;
    func_801D1E80();
    while (D_800625A0->viewMotion != 0) {
        func_801C7BF4();
    }
    func_801D22F4(0);
    found = func_801CACF8(0x7d, 0xff, 1);
    if ((found || D_801EA8FC != 0) && func_801CACF8(0x80, 0xff, 1) != 0) {
        save = 0;
    }
    func_801D2484();
    if (save) {
        D_801E96A5 = 1;
        D_801E96A4 = 1;
        func_801C531C(0);
        D_801E96A4 = 0;
        D_801E96A5 = 0;
    }
    D_800625A0->party->redraw4 = 0;
    D_800625A0->party->redraw3 = 0;
    func_801C8694(D_80059171);
}

/* The title screen's file screen loop: choose among its three commands; after
 * 600 idle frames on the first screen it returns with D_800594D0 = 1. */
void func_801C58EC(void) {
    u8 stay;

    stay = 1;
    func_801E8474(4, D_801EA1D4);
    func_801E8018(8, D_800625A0->commandLabels, D_801EA530, D_800625A0->party->labels);
    D_800625A0->frameCounter = 0;
    do {
        func_801C7BF4();
        switch (D_800625A0->input) {
        case 4:
            D_800625A0->screenImages->captured = 1;
            func_801D22C4();
            func_801E8044(8, D_800625A0->party->labels);
            D_800625A0->primitives->shade = 0x40;
            stay = func_801C531C(7);
            D_801E9784 = 0;
            D_800625A0->frameCounter = 0;
            break;
        case 1:
            if (D_800625A0->cursor != 0) {
                D_800625A0->cursor--;
            } else {
                D_800625A0->cursor = 2;
            }
            D_800625A0->frameCounter = 0;
            break;
        case 3:
            if (++D_800625A0->cursor >= 3) {
                D_800625A0->cursor = 0;
            }
            D_800625A0->frameCounter = 0;
            break;
        }
        if (D_800625A0->cursor != D_800625A0->cursorShown) {
            func_801E8978(3, D_800625A0->cursor, D_801EA1D4);
            func_801E8070(8, D_800625A0->commandLabels, D_801EA530, D_801E9E84, D_800625A0->party->labels,
                          D_800625A0->cursor, 0, 0);
            D_800625A0->cursorShown = D_800625A0->cursor;
        }
        if (func_80028530() == 1 && (u32)D_800625A0->frameCounter > 600) {
            stay = 0;
            D_800594D0 = 1;
        }
    } while (stay);
    func_801E8044(8, D_800625A0->party->labels);
}

/* Allocate and clear (nonzero) or free (zero) the memory-card state block. */
void func_801C5B54(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x5034, 0);
        D_800625A0->card = block;
        bzero(block, 0x5034);
    } else {
        func_800320E8(D_800625A0->card);
    }
}

/* Allocate and clear (nonzero) or free (zero) the party block. */
void func_801C5BB8(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x6c, 0);
        D_800625A0->party = block;
        bzero(block, 0x6c);
    } else {
        func_800320E8(D_800625A0->party);
    }
}

/* Allocate and clear (nonzero) or free (zero) the screen image block. */
void func_801C5C1C(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x1194, 0);
        D_800625A0->screenImages = block;
        bzero(block, 0x1194);
    } else {
        func_800320E8(D_800625A0->screenImages);
    }
}

/* Allocate and clear (nonzero) or free (zero) the sprite lists (+354). */
void func_801C5C80(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x140c, 0);
        D_800625A0->spriteLists = block;
        bzero(block, 0x140c);
    } else {
        func_800320E8(D_800625A0->spriteLists);
    }
}

/* Allocate and clear (nonzero) or free (zero) the data table directory. */
void func_801C5CE4(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0xcc, 0);
        D_800625A0->tables = block;
        bzero(block, 0xcc);
    } else {
        func_800320E8(D_800625A0->tables);
    }
}

/* Allocate and clear (nonzero) or free (zero) the first field-menu block. */
void func_801C5D48(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x328, 0);
        D_800625A0->fieldMenu = block;
        bzero(block, 0x328);
    } else {
        func_800320E8(D_800625A0->fieldMenu);
    }
}

/* Allocate and clear (nonzero) or free (zero) the second field-menu block. */
void func_801C5DAC(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x374, 0);
        D_800625A0->fieldMenu2 = block;
        bzero(block, 0x374);
    } else {
        func_800320E8(D_800625A0->fieldMenu2);
    }
}

/* Allocate and clear (nonzero) or free (zero) the shared primitive block. */
void func_801C5E10(u8 allocate) {
    if (allocate) {
        void *block = func_80031BDC(0x15c, 0);
        D_800625A0->primitives = block;
        bzero(block, 0x15c);
    } else {
        func_800320E8(D_800625A0->primitives);
    }
}

/* Allocate and clear (nonzero) or free (zero) the three field blocks. */
void func_801C5E74(u8 allocate) {
    s32 i;

    if (allocate) {
        for (i = 0; i < 3; i++) {
            void *block = func_80031BDC(0x127c, 0);
            D_800625A0->fieldBlocks[i] = block;
            bzero(block, 0x127c);
        }
    } else {
        for (i = 0; i < 3; i++) {
            func_800320E8(D_800625A0->fieldBlocks[i]);
        }
    }
}

/* Allocate the blocks of the menu kind in D_80059460 (the card state for the
 * title file screen and kind 6; the card state and field blocks for the field
 * menu). */
void func_801C5F10(void) {
    void *block;

    func_801C5BB8(1);
    func_801C5C1C(1);
    func_801C5C80(1);
    func_801C5CE4(1);
    func_801C5E10(1);
    block = func_80031BDC(0x14c, 0);
    D_800625A0->markers = block;
    bzero(block, 0x14c);
    switch (D_80059460) {
    case 0:
        func_801C5B54(1);
        func_801C5D48(1);
        func_801C5DAC(1);
        func_801C5E74(1);
        break;
    case 2:
    case 6:
        func_801C5B54(1);
        break;
    }
}

/* Leave the menu: keep the field menu's cursor, stop drawing, free the sound
 * bank and every block, then the state itself. */
void func_801C5FE4(void) {
    switch (D_80059460) {
    case 0:
        func_801D22C4();
        func_801D29A8(0, 0);
        D_800625A0->party->redraw6 = 0;
        D_800625A0->party->redraw5 = 0;
        if (D_80059171 == 0) {
            D_800594CC = D_800625A0->cursor;
        }
        break;
    case 2:
    case 6:
        break;
    }
    func_801C7BF4();
    func_801C7BF4();
    D_800625A0->drawing = 0;
    func_801C7BF4();
    do {
        func_801C7BF4();
    } while (D_800625A0->bufferIndex != 0);
    func_801C5BB8(0);
    func_801C5C1C(0);
    func_801C5C80(0);
    func_801C5CE4(0);
    func_801C5E10(0);
    func_800320E8(D_800625A0->markers);
    func_800320E8(D_800625A0->sheet);
    func_800320E8(D_800625A0->labels);
    func_800320E8(D_800625A0->topLabels[0].pixels);
    if (D_80059178 != 0) {
        func_8003A094(D_800625A0->effectBank);
        func_801C7BF4();
        func_8003852C(D_800625A0->effectBank);
        func_801C7BF4();
        func_800320E8(D_800625A0->effectBank);
    }
    switch (D_80059460) {
    case 0:
        func_801C5B54(0);
        func_801C5D48(0);
        func_801C5DAC(0);
        func_801C5E74(0);
        func_800320E8(D_800625A0->portraits[0]);
        func_800320E8(D_800625A0->portraitMarks[0]);
        func_800320E8(D_800625A0->portraits[1]);
        func_800320E8(D_800625A0->portraitMarks[1]);
        break;
    case 2:
    case 6:
        func_801C5B54(0);
        break;
    }
    func_800320E8(D_800625A0);
}

/* The menu mode: allocate, set up the screen, run the menu of kind
 * D_80059460 (then, after the title file screen, the disc check of the loaded
 * file) and tear down. */
void func_801C62A8(void) {
    u8 kind;

    func_801C5F10();
    func_801C7B0C();
    D_800625A0->drawing = 1;
    D_800625A0->sounds = 1;
    kind = D_80059460;
    switch (kind) {
    case 0:
        func_801D2D38();
        func_801C55A0();
        break;
    case 2:
        D_800594D0 = 0;
        func_801C58EC();
        D_800625A0->party->redraw9 = 0;
        D_800625A0->party->redraw4 = 0;
        D_800625A0->party->redraw3 = 0;
        switch (D_800594D0) {
        case 0:
            func_801C8694(0);
            break;
        case 2:
            func_801C8694(D_8006F008);
            break;
        }
        break;
    case 6:
        func_801C57A4();
        break;
    }
    func_801C5FE4();
}

/* Initialize the card scan and read the scenario's title from its text
 * file, skipping complete lines and two-byte characters. */
void func_801C6400(void) {
    u8 *file;
    u8 *text;
    s32 i;
    s32 j;
    u16 line;
    u8 c;
    s32 newline;
    s32 decrement;

    for (i = 0; i < 2; i++) {
        D_800625A0->card->scanned[i] = 0;
        D_800625A0->card->unk4F8A[i] = 0;
        D_800625A0->card->unk4F8C[i] = 0xff;
    }
    D_800625A0->card->mode = 0;
    for (i = 0; i < 32; i++) {
        D_800625A0->card->files[i].state = 0;
        D_800625A0->card->fileSlots[i] = 0xff;
    }
    line = D_8006EF64;
    i = 0;
    func_80028470(0x10, 1);
    file = func_80031BDC(func_800288EC(1), 1);
    func_800295D8(1, file, 0, 0x80);
    func_80028A60(0);
    text = file;
    if (line != 0) {
        decrement = 0xFFFF;
        newline = '\n';
        do {
        next:
            c = text[i];
            if (c >= 0x80) {
                i += 2;
                goto next;
            }
            if (c != newline) {
                i += 1;
                goto next;
            }
            line += decrement;
            i += 1;
        } while (line != 0);
    }
    for (j = 0; j < 30; j++) {
        D_800625A0->card->title[j] = text[i++];
    }
    D_800625A0->card->unk501A = D_800625A0->card->unk501B = 0;
    func_80028470(0x10, 0);
    func_800320E8(text);
}

/* Load the menu resources: the card file header template (name prefixes,
 * "SC" header, icon palette and pixels), the TIM list, sprite sheet and
 * label text, the party's portraits and, with sound, the effect bank. */
void func_801C65F4(void) {
    TIM_IMAGE tim;
    s32 tex[3 * 6]; /* per entry 80026338's six outputs: -, mode, clut x/y, page x/y */
    MenuResources *res;
    void *data;
    s32 i;
    u8 id;

    res = D_8005945C;
    func_8003342C(res);
    data = func_80032E88(res->files[0], 1);
    OpenTIM(data);
    ReadTIM(&D_800625A0->card->icon);
    strcpy(D_800625A0->card->prefix, "BASLUS-00664");
    strcpy((char *)D_800625A0->card->otherPrefix, "BASLUS-01160");
    D_800625A0->card->saveMagic[0] = 'S';
    D_800625A0->card->saveMagic[1] = 'C';
    D_800625A0->card->saveIconFlag = 0x11;
    D_800625A0->card->saveBlocks = 1;
    bzero(D_800625A0->card->saveTitle, sizeof(D_800625A0->card->saveTitle));
    memmove(D_800625A0->card->savePalette, D_800625A0->card->icon.caddr, sizeof(D_800625A0->card->savePalette));
    memmove(D_800625A0->card->saveIcon, D_800625A0->card->icon.paddr, sizeof(D_800625A0->card->saveIcon));
    func_800320E8(data);
    data = func_80032E88(res->files[1], 1);
    func_8002DD20(data);
    func_800320E8(data);
    D_800625A0->sheet = func_80032E88(res->files[2], 0);
    D_800625A0->labels = func_80032E88(res->files[3], 0);
    func_80026338(D_800625A0->sheet, 0xe0, &tex[0], &tex[1], &tex[2],
                  &tex[3], &tex[4], &tex[5]);
    func_80026338(D_800625A0->sheet, 0x14b, &tex[0], &tex[1], &tex[2],
                  &tex[3], &tex[4], &tex[5]);
    func_80026338(D_800625A0->sheet, 0x14c, &tex[6], &tex[7], &tex[8],
                  &tex[9], &tex[10], &tex[11]);
    func_80026338(D_800625A0->sheet, 0x14d, &tex[12], &tex[13], &tex[14],
                  &tex[15], &tex[16], &tex[17]);
    tex[10] += 0xc;
    data = func_80032E88(res->files[4], 1);
    for (i = 0; i < 3; i++) {
        id = D_800625A0->party->ids[i];
        if (id != 0xff) {
            OpenTIM((u8 *)data + id * 0xb20);
            ReadTIM(&tim);
            tim.crect->x = tex[i * 6 + 2];
            tim.crect->y = tex[i * 6 + 3];
            tim.prect->x = tex[i * 6 + 4];
            tim.prect->y = tex[i * 6 + 5];
            LoadImage(tim.crect, tim.caddr);
            LoadImage(tim.prect, tim.paddr);
        }
    }
    DrawSync(0);
    func_800320E8(data);
    if (D_80059178 != 0) {
        func_80028470(0x10, 2);
        D_8006259C = func_80031BDC(func_800288EC(5), 0);
        func_800295D8(5, D_8006259C, 0, 0x80);
        func_80028A60(0);
        func_80028470(0x10, 0);
        func_80038428(D_8006259C);
    }
    D_800625A0->effectBank = D_8006259C;
    func_800320E8(res);
}

/* Set up the party: the starting top cursor, which characters are present,
 * the party slots (and which have a gear), the first occupied slot; then
 * load the resources. */
void func_801C6AA0(MenuState *state) {
    s32 i;
    u16 members;
    s32 id;

    if (D_80059460 == 0 && D_80059171 == 0) {
        D_800625A0->cursor = D_800594CC;
    } else {
        D_800625A0->cursor = 1;
    }
    D_800625A0->cursorShown = 0xff;
    D_800625A0->cardPollTimer = 0x3c;
    D_800625A0->cardsPresent = 0;
    D_800625A0->unk335 = 0;
    D_800625A0->partyCount = 0;
    members = D_8006F364 & D_8006F366 & 0x7ff;
    for (i = 0; i < 16; i++) {
        if (func_801C865C(members, i)) {
            D_800625A0->present[i] = 1;
        } else {
            D_800625A0->present[i] = 0;
        }
    }
    for (i = 0; i < 3; i++) {
        D_800625A0->party->ready[i] = 0;
        id = D_8006F368[i];
        if (id != 0xff && D_800625A0->present[id]) {
            D_800625A0->party->ids[i] = id;
            D_800625A0->partyCount++;
            if (D_8006D8A0[D_800625A0->party->ids[i]].gear != 0xff) {
                D_800625A0->party->ready[i] = 1;
                D_800625A0->fighters++;
            }
        } else {
            D_800625A0->party->ids[i] = 0xff;
        }
    }
    for (i = 0; i < 3; i++) {
        if (D_800625A0->party->ids[i] != 0xff) {
            D_800625A0->firstMember = i;
            break;
        }
    }
    func_801C65F4();
}

/* Mark the buffer being built as sent (the draw callback). */
void func_801C6D4C(void) {
    D_800625A0->bufferIndex = 0;
}

/* Clear the resource load state. */
void func_801C6D5C(void) {
    D_800625A0->unk4CC = 0;
    D_800625A0->unk4D0 = 0;
    D_800625A0->loadState = 0;
    D_800625A0->unk4D9 = 0;
    D_800625A0->unk4D4 = 0;
}

/* Upload a 16-entry palette at (0, 1c0) whose entry 1 is white. */
void func_801C6D90(void) {
    RECT rect;
    u8 reserved[8]; /* the original frame reserves 8 unused bytes */
    u16 *clut;

    clut = func_80031BDC(0x20, 0);
    bzero(clut, 0x20);
    clut[1] = 0x7fff;
    rect.x = 0;
    rect.y = 0x1c0;
    rect.w = 0x10;
    rect.h = 1;
    LoadImage(&rect, clut);
    DrawSync(0);
    func_800320E8(clut);
}

/* Set up the label text: the font position, the label pixel block and the
 * four label image records, and the label palette. */
void func_801C6E0C(void) {
    func_80033698(0, 0x1d1);
    D_800625A0->topLabels[0].pixels = func_80031BDC(0x38e, 0);
    func_801E7E68(D_800625A0->topLabels, D_801EA524, 0, 4);
    func_801C6D90();
}

/* Read the four sprite sheet records used by the menu. */
void func_801C6E68(void) {
    u8 reserved[40]; /* the original frame reserves 40 unused bytes */

    func_80026338(D_800625A0->sheet, 0xfe, &D_800625A0->sheetEntries[0][0], &D_800625A0->sheetEntries[0][1],
                  &D_800625A0->sheetEntries[0][2], &D_800625A0->sheetEntries[0][3],
                  &D_800625A0->sheetEntries[0][4], &D_800625A0->sheetEntries[0][5]);
    func_80026338(D_800625A0->sheet, 0x103, &D_800625A0->sheetEntries[1][0], &D_800625A0->sheetEntries[1][1],
                  &D_800625A0->sheetEntries[1][2], &D_800625A0->sheetEntries[1][3],
                  &D_800625A0->sheetEntries[1][4], &D_800625A0->sheetEntries[1][5]);
    func_80026338(D_800625A0->sheet, 0x100, &D_800625A0->sheetEntries[2][0], &D_800625A0->sheetEntries[2][1],
                  &D_800625A0->sheetEntries[2][2], &D_800625A0->sheetEntries[2][3],
                  &D_800625A0->sheetEntries[2][4], &D_800625A0->sheetEntries[2][5]);
    func_80026338(D_800625A0->sheet, 0x101, &D_800625A0->sheetEntries[3][0], &D_800625A0->sheetEntries[3][1],
                  &D_800625A0->sheetEntries[3][2], &D_800625A0->sheetEntries[3][3],
                  &D_800625A0->sheetEntries[3][4], &D_800625A0->sheetEntries[3][5]);
}

/* Set up the frame primitives of both buffers: the shaded backdrop, two dark
 * green separator lines, the grey full-screen fade and two draw modes. */
void func_801C6F70(void) {
    RECT window;
    s32 i;

    window.y = 0;
    window.x = 0;
    window.h = 0x100;
    window.w = 0x100;
    func_801D22C4();
    for (i = 0; i < 2; i++) {
        func_801C8164(&D_800625A0->primitives->box[i], 0x80, 0x80, 0);
        SetSemiTrans(&D_800625A0->primitives->box[i], 1);
        SetLineF3(&D_800625A0->primitives->edgeA[i]);
        setRGB0(&D_800625A0->primitives->edgeA[i], 0, 0x40, 0);
        SetLineF3(&D_800625A0->primitives->edgeB[i]);
        setRGB0(&D_800625A0->primitives->edgeB[i], 0, 0x40, 0);
        SetPolyF4(&D_800625A0->primitives->fills[i]);
        setXY4(&D_800625A0->primitives->fills[i], 0, 0, 0x140, 0, 0, 0xe0, 0x140, 0xe0);
        setRGB0(&D_800625A0->primitives->fills[i], 0x80, 0x80, 0x80);
        SetSemiTrans(&D_800625A0->primitives->fills[i], 1);
        SetDrawMode(&D_800625A0->primitives->modes0[i], 0, 0, GetTPage(0, 0, 0x140, 0x80), &window);
        SetDrawMode(&D_800625A0->primitives->modes[i], 0, 0, GetTPage(0, 2, 0x180, 0), &window);
    }
}

/* Load (codes below 10h, read from file 2 of directory 10h) or release
 * (10h + the same code) the menu data set `code` into the table directory and
 * the screen blocks. */
void func_801C72BC(u8 code) {
    MenuDataArchive *archive;
    s32 i;
    u8 id;
    u8 gear;

    if (code < 0x10) {
        func_80028470(0x10, 0);
        archive = func_80031BDC(func_800288EC(2), 1);
        func_800295D8(2, archive, 0, 0x80);
        func_80028A60(0);
        func_8003342C(archive);
    }
    switch (code) {
    case 0:
        D_800625A0->tables->items = func_80032E88(archive->items, 0);
        D_800625A0->block42C->unk1180 = func_80032E88(archive->unk3C, 0);
        break;
    case 1:
        D_800625A0->tables->engines = func_80032E88(archive->engines, 0);
        D_800625A0->tables->frames = func_80032E88(archive->frames, 0);
        D_800625A0->tables->parts = func_80032E88(archive->parts, 0);
        D_800625A0->tables->gearAccessories = func_80032E88(archive->unk50, 0);
        break;
    case 2:
        for (i = 0; i < 3; i++) {
            id = D_800625A0->party->ids[i];
            if (id != 0xff) {
                D_800625A0->tables->effects[D_800625A0->party->ids[i]] = func_80032E88(archive->effects[id], 0);
            }
        }
        D_800625A0->block430->texts = func_80032E88(archive->unk40, 0);
        break;
    case 3:
        D_800625A0->tables->weapons = func_80032E88(archive->weapons, 0);
        D_800625A0->tables->accessories = func_80032E88(archive->accessories, 0);
        D_800625A0->tables->gearWeapons = func_80032E88(archive->unkAC, 0);
        D_800625A0->tables->gearAccessories = func_80032E88(archive->unk50, 0);
        break;
    case 4:
        for (i = 0; i < 3; i++) {
            id = D_800625A0->party->ids[i];
            if (id != 0xff) {
                D_800625A0->tables->effects[D_800625A0->party->ids[i]] = func_80032E88(archive->effects[id], 0);
            }
        }
        D_800625A0->block438->unk2578 = func_80032E88(archive->unkB0, 0);
        break;
    case 5:
    case 6:
        for (i = 0; i < 3; i++) {
            id = D_800625A0->party->ids[i];
            if (id != 0xff) {
                gear = D_8006D8A0[id].gear;
                if (gear != 0xff) {
                    (D_800625A0->tables->effects + 11)[D_8006D8A0[D_800625A0->party->ids[i]].gear] =
                        func_80032E88(archive->gears[gear], 0);
                    func_801E4998((MenuGearViews *)D_800625A0->tables, D_8006D8A0[D_800625A0->party->ids[i]].gear);
                }
            }
        }
        if (code == 5) {
            D_800625A0->block430->texts = func_80032E88(archive->unk54, 0);
        } else {
            D_800625A0->block430->texts = func_80032E88(archive->unk58, 0);
        }
        break;
    case 7:
        D_800625A0->block434->texts[0] = func_80032E88(archive->unkD4[0], 0);
        D_800625A0->block434->texts[1] = func_80032E88(archive->unkD4[1], 0);
        D_800625A0->block434->texts[2] = func_80032E88(archive->unkD4[2], 0);
        D_800625A0->block434->texts[3] = func_80032E88(archive->unkD4[3], 0);
        break;
    case 0x10:
        func_800320E8(D_800625A0->tables->items);
        func_800320E8(D_800625A0->block42C->unk1180);
        break;
    case 0x11:
        func_800320E8(D_800625A0->tables->engines);
        func_800320E8(D_800625A0->tables->frames);
        func_800320E8(D_800625A0->tables->parts);
        func_800320E8(D_800625A0->tables->gearAccessories);
        break;
    case 0x12:
        for (i = 0; i < 3; i++) {
            id = D_800625A0->party->ids[i];
            if (id != 0xff) {
                func_800320E8(D_800625A0->tables->effects[id]);
            }
        }
        func_800320E8(D_800625A0->block430->texts);
        break;
    case 0x13:
        func_800320E8(D_800625A0->tables->weapons);
        func_800320E8(D_800625A0->tables->accessories);
        func_800320E8(D_800625A0->tables->gearWeapons);
        func_800320E8(D_800625A0->tables->gearAccessories);
        break;
    case 0x14:
        for (i = 0; i < 3; i++) {
            id = D_800625A0->party->ids[i];
            if (id != 0xff) {
                func_800320E8(D_800625A0->tables->effects[id]);
            }
        }
        func_800320E8(D_800625A0->block438->unk2578);
        break;
    case 0x15:
    case 0x16:
        for (i = 0; i < 3; i++) {
            id = D_800625A0->party->ids[i];
            if (id != 0xff) {
                gear = D_8006D8A0[id].gear;
                if (gear != 0xff) {
                    func_800320E8((D_800625A0->tables->effects + 11)[gear]);
                }
            }
        }
        func_800320E8(D_800625A0->block430->texts);
        break;
    case 0x17:
        func_800320E8(D_800625A0->block434->texts[0]);
        func_800320E8(D_800625A0->block434->texts[1]);
        func_800320E8(D_800625A0->block434->texts[2]);
        func_800320E8(D_800625A0->block434->texts[3]);
        break;
    }
    if (code < 0x10) {
        func_800320E8(archive);
    }
}

/* Set up the screen: the copied screen area, the environments, labels,
 * palettes and sheet records, then the card state and load state. */
void func_801C7B0C(void) {
    MenuState *state = D_800625A0;

    state->screenImages->copy.x = 0x2c0;
    state->screenImages->copy.y = 0x100;
    state->screenImages->copy.w = 0x140;
    state->screenImages->copy.h = 0xe0;
    state->primitives->shade = 0x40;
    func_801C6AA0(state);
    func_801C6D4C();
    func_801C6E0C();
    func_801C6F70();
    func_801C6E68();
    switch (D_80059460) {
    case 2:
        D_800625A0->primitives->shade = 0x4c;
    case 0:
    case 6:
        func_801C6400();
        func_801C6D5C();
        break;
    }
}

/* One menu frame: poll the pads, swap and clear the buffers, build the frame
 * (updates, input, sounds), wait for the previous frame, show this one and copy
 * the screen area into the other buffer. */
void func_801C7BF4(void) {
    MenuBuffer *buffer;
    s32 other;

    if (*D_8005917C != -1) {
        /* ASPSX "break 1": code 1 in the upper code field (0x400 for maspsx) */
        __asm__ volatile("break 0x400");
    }
    func_801C7D78();
    if (D_801E9784 != 0) {
        func_80019CA0();
    }
    buffer = &D_800625A0->buffers[0];
    if (D_800625A0->current == buffer) {
        buffer = &D_800625A0->buffers[1];
    }
    D_800625A0->current = buffer;
    D_800625A0->bufferIndex = D_800625A0->bufferIndex == 0;
    ClearOTagR(D_800625A0->current->ot, 16);
    func_8001BD40(0, 0xff);
    func_801D1D40();
    D_800625A0->frameCounter++;
    func_801C7F34(D_80059488);
    func_801D2968();
    func_801D1CA0();
    other = D_800625A0->bufferIndex == 0;
    DrawSync(0);
    VSync(0);
    PutDrawEnv(D_800625A0->current->draw);
    PutDispEnv(D_800625A0->current->disp);
    MoveImage(&D_800625A0->screenImages->copy, 0, other * 0xe0);
    DrawOTag(&D_800625A0->current->ot[15]);
    func_801C8BEC();
    func_801C8EE8();
}

/* Decode this frame's input into D_800625A0->input (0-3 directions, 4-7 the
 * face buttons, 9/10 the shoulder buttons, 12 select, 8 none), playing the
 * cursor sounds; while the pad is disconnected, pause the sound and the play
 * time. */
void func_801C7D78(void) {
    u8 waiting;
    u8 paused;
    s32 frames;
    u8 input;

    waiting = 1;
    paused = 0;
    do {
        if (func_80035734(0) == 0) {
            if (!paused) {
                paused++;
                func_80037EE4();
                frames = D_80059488;
            }
        } else {
            waiting--;
            if (paused) {
                func_80037E8C();
                D_80059488 = frames;
            }
        }
    } while (waiting);
    input = 8;
    if (func_80036410() != 0) {
        func_80035DB0();
    } else {
        while (func_80035CDC() != 0) {
            if (D_800594A4 & 0x2000) {
                input = 0;
                func_801C8574(1);
                break;
            }
            if (D_800594A4 & 0x4000) {
                input = 1;
                func_801C8574(1);
                break;
            }
            if (D_800594A4 & 0x8000) {
                input = 2;
                func_801C8574(1);
                break;
            }
            if (D_800594A4 & 0x1000) {
                input = 3;
                func_801C8574(1);
                break;
            }
            if (D_8005948C & 0x20) {
                input = 4;
                func_801C8574(2);
                break;
            }
            if (D_8005948C & 0x40) {
                input = 5;
                func_801C8574(3);
                break;
            }
            if (D_8005948C & 0x80) {
                input = 6;
                break;
            }
            if (D_8005948C & 0x10) {
                input = 7;
                break;
            }
            if (D_800594A4 & 4) {
                input = 10;
                func_801C8574(1);
                break;
            }
            if (D_800594A4 & 8) {
                input = 9;
                func_801C8574(1);
                break;
            }
            if (D_8005948C & 0x100) {
                input = 12;
                break;
            }
        }
    }
    D_800625A0->input = input;
}

/* Split the play time `frames` into the digits of hhh:mm:ss (the hundreds,
 * tens and units of hours, then tens and units of minutes and seconds). */
void func_801C7F34(u32 frames) {
    D_800625A0->time[0] = frames / 21600000;
    frames %= 21600000;
    D_800625A0->time[1] = frames / 2160000;
    frames %= 2160000;
    D_800625A0->time[2] = frames / 216000;
    frames %= 216000;
    D_800625A0->time[3] = frames / 36000;
    frames %= 36000;
    D_800625A0->time[4] = frames / 3600;
    frames %= 3600;
    D_800625A0->time[5] = frames / 600;
    frames %= 600;
    D_800625A0->time[6] = frames / 60;
}

/* Split `value` into nine decimal digits, blanking (ff) the leading zeros. */
void func_801C80B8(u32 value) {
    u32 unit;
    s32 i;

    unit = 100000000;
    for (i = 0; i < 9; i++) {
        D_800625A0->digits[i] = value / unit;
        value %= unit;
        unit /= 10;
    }
    for (i = 1; i < 9; i++) {
        if (D_800625A0->digits[i] != 0) {
            if (D_800625A0->digits[i - 1] == 0) {
                D_800625A0->digits[i - 1] = 0xff;
            }
            return;
        }
        D_800625A0->digits[i - 1] = 0xff;
    }
}

/* Initialise a gradient quad: the top edge colour (r, g, b), the bottom black. */
void func_801C8164(POLY_G4 *poly, u8 r, u8 g, u8 b) {
    SetPolyG4(poly);
    poly->r0 = r;
    poly->g0 = g;
    poly->b0 = b;
    poly->r1 = r;
    poly->g1 = g;
    poly->b1 = b;
    poly->r2 = 0;
    poly->g2 = 0;
    poly->b2 = 0;
    poly->r3 = 0;
    poly->g3 = 0;
    poly->b3 = 0;
}

/* Start mover `slot` along the line (x0, y0)-(x1, y1) at `speed` steps per
 * frame: the major axis steps one unit (8.8 fixed point), the minor axis its
 * slope. */
void func_801C81E0(s32 x0, s32 y0, s32 x1, s32 y1, u8 speed, u8 slot) {
    s32 dx;
    s32 dy;

    D_800625A0->movers[slot].x0 = x0;
    D_800625A0->movers[slot].y0 = y0;
    D_800625A0->movers[slot].x1 = x1;
    D_800625A0->movers[slot].y1 = y1;
    if (x1 < x0) {
        dx = x0 - x1;
        D_800625A0->movers[slot].negX = 1;
    } else {
        dx = x1 - x0;
        D_800625A0->movers[slot].negX = 0;
    }
    if (y1 < y0) {
        dy = y0 - y1;
        D_800625A0->movers[slot].negY = 1;
    } else {
        dy = y1 - y0;
        D_800625A0->movers[slot].negY = 0;
    }
    if (dx >= dy) {
        D_800625A0->movers[slot].stepX = 0x100;
        D_800625A0->movers[slot].stepY = (dy << 8) / dx;
    } else {
        D_800625A0->movers[slot].stepY = 0x100;
        D_800625A0->movers[slot].stepX = (dx << 8) / dy;
    }
    D_800625A0->movers[slot].speed = speed;
    D_800625A0->movers[slot].accX = 0;
    D_800625A0->movers[slot].accY = 0;
    D_800625A0->movers[slot].done = 0;
}

/* Advance mover `slot` by its speed and mark it done once the major axis
 * passes the end point. */
void func_801C8324(u8 slot) {
    s32 i;

    for (i = 0; i < D_800625A0->movers[slot].speed; i++) {
        if (D_800625A0->movers[slot].negX) {
            D_800625A0->movers[slot].accX -= D_800625A0->movers[slot].stepX;
        } else {
            D_800625A0->movers[slot].accX += D_800625A0->movers[slot].stepX;
        }
        if (D_800625A0->movers[slot].negY) {
            D_800625A0->movers[slot].accY -= D_800625A0->movers[slot].stepY;
        } else {
            D_800625A0->movers[slot].accY += D_800625A0->movers[slot].stepY;
        }
    }
    if (D_800625A0->movers[slot].stepX == 0x100) {
        if (D_800625A0->movers[slot].negX) {
            if (D_800625A0->movers[slot].accX / 256 + D_800625A0->movers[slot].x0 < D_800625A0->movers[slot].x1) {
                D_800625A0->movers[slot].done = 1;
            }
        } else if (D_800625A0->movers[slot].x1 < D_800625A0->movers[slot].accX / 256 + D_800625A0->movers[slot].x0) {
            D_800625A0->movers[slot].done = 1;
        }
    } else if (D_800625A0->movers[slot].negY) {
        if (D_800625A0->movers[slot].accY / 256 + D_800625A0->movers[slot].y0 < D_800625A0->movers[slot].y1) {
            D_800625A0->movers[slot].done = 1;
        }
    } else if (D_800625A0->movers[slot].y1 < D_800625A0->movers[slot].accY / 256 + D_800625A0->movers[slot].y0) {
        D_800625A0->movers[slot].done = 1;
    }
}

/* Set the four corners of a screen rectangle as vertices centred on (a0, 70). */
void func_801C851C(SVECTOR *v, u16 x, u16 y, u16 w, u16 h) {
    v[0].vx = x - 0xa0;
    v[0].vy = y - 0x70;
    v[0].vz = 0;
    v[1].vx = x + w - 0xa0;
    v[1].vy = y - 0x70;
    v[1].vz = 0;
    v[2].vx = x - 0xa0;
    v[2].vz = 0;
    v[3].vx = x + w - 0xa0;
    v[3].vz = 0;
    v[2].vy = y + h - 0x70;
    v[3].vy = y + h - 0x70;
}

/* Play menu sound effect `sound` from the menu's bank when sounds are on. */
void func_801C8574(s32 sound) {
    if (D_800625A0->sounds != 0) {
        func_80039DB8((D_800625A0->effectBank->id << 16) | (u8)sound, sound);
    }
}

/* Bit `bit` of the D_801E96C8 masks. */
u16 func_801C85C0(u8 bit) {
    return D_801E96C8[bit];
}

/* Bit `bit` of the D_801E96A8 masks. */
u16 func_801C85DC(u8 bit) {
    return D_801E96A8[bit];
}

/* All bits but `bit` (D_801E96C8). */
u16 func_801C85F8(u8 bit) {
    return ~D_801E96C8[bit];
}

/* All bits but `bit` (D_801E96A8). */
u16 func_801C861C(u8 bit) {
    return ~D_801E96A8[bit];
}

/* Test bit `bit` (D_801E96C8) of `flags`. */
u16 func_801C8640(u16 flags, u8 bit) {
    return D_801E96C8[bit] & flags;
}

/* Test bit `bit` (D_801E96A8) of `flags`. */
u16 func_801C865C(u16 flags, u8 bit) {
    return D_801E96A8[bit] & flags;
}

/* Test bit `bit` (D_801E96E8) of `flags`. */
u32 func_801C8678(u32 flags, u8 bit) {
    return flags & D_801E96E8[bit];
}

/* Ask for disc `disc` + 1 until it is in the drive, showing the change-disc
 * notice (and, for a wrong disc, the wrong-disc message for 29 frames). */
void func_801C8694(u8 disc) {
    u8 checking;
    u8 frames;

    func_801D1E80();
    checking = 1;
    while (D_800625A0->viewMotion != 0) {
        func_801C7BF4();
    }
    func_801D22F4(0);
    while (checking) {
        if (func_80028530() == disc + 1) {
            checking = 0;
        } else {
            func_801E92CC();
            func_801D2F4C(disc * 3 - 0x7d);
            if (func_801E93A0(disc + 1) != 0) {
                frames = 0x1d;
                func_801D32B4();
                func_801D2F4C(0x89);
                do {
                    frames--;
                    func_801C7BF4();
                } while (frames);
                func_801D32B4();
                func_801C7BF4();
            } else {
                func_801D32B4();
                checking = 0;
            }
        }
    }
    func_801D2484();
}

/* Discard pending memory-card events. */
void func_801C87C4(void) {
    UnDeliverEvent(0xf4000001, 4);
    UnDeliverEvent(0xf4000001, 0x8000);
    UnDeliverEvent(0xf4000001, 0x100);
    UnDeliverEvent(0xf4000001, 0x2000);
}

/* Wait for a memory-card event; returns 0 done, 1 error, 2 timeout, 3 new card. */
u8 func_801C881C(void) {
    for (;;) {
        if (TestEvent(D_800625A0->card->events[3]) == 1) {
            func_801C87C4();
            return 3;
        }
        if (TestEvent(D_800625A0->card->events[1]) == 1) {
            func_801C87C4();
            return 1;
        }
        if (TestEvent(D_800625A0->card->events[0]) == 1) {
            func_801C87C4();
            return 0;
        }
        if (TestEvent(D_800625A0->card->events[2]) == 1) {
            func_801C87C4();
            return 2;
        }
    }
}

/* Start a card check on `channel` and wait for its event: the D_801E9768
 * result for it, or -1 when the check cannot start. */
s32 func_801C891C(s32 channel) {
    if (func_8004E784(channel) == 0) {
        return -1;
    }
    return D_801E9768[func_801C881C()];
}

/* Finish a frame, then enable the four card events in a critical section. */
void func_801C8960(void) {
    func_801C7BF4();
    EnterCriticalSection();
    CloseEvent(D_800625A0->card->events[0]);
    CloseEvent(D_800625A0->card->events[1]);
    CloseEvent(D_800625A0->card->events[2]);
    CloseEvent(D_800625A0->card->events[3]);
    ExitCriticalSection();
}

/* Check the card in `port`; a removed card clears its file listing. Returns 0
 * when the check could not run (-2). */
u8 func_801C8A10(u8 port) {
    u8 ok;
    s32 result;
    s32 i;

    ok = 1;
    D_800625A0->card->present[port] = 1;
    result = func_801C891C(port ? 0x10 : 0);
    if (result == 0 && D_800625A0->card->result[port] == -1) {
        result = 1;
        D_800625A0->card->result[port] = 0;
    } else {
        D_800625A0->card->result[port] = result;
    }
    if (result == -1) {
        D_800625A0->card->unk4F8A[port] = 0;
        D_800625A0->card->present[port] = 0;
        D_801EA900[port] = 0;
        for (i = 0; i < 16; i++) {
            D_800625A0->card->fileSlots[port * 16 + i] = 0xff;
            D_800625A0->card->ours[port * 16 + i] = 0;
            D_800625A0->card->files[port * 16 + i].state = 0;
        }
    }
    if (result == -2) {
        ok = 0;
    }
    if (D_800625A0->card->presentShown[port] != D_800625A0->card->present[port]) {
        D_800625A0->card->scanned[port] = 0;
        D_800625A0->card->presentShown[port] = D_800625A0->card->present[port];
    }
    return ok;
}

/* While the card screen is up, recheck both ports every D_801E9779 frames. */
void func_801C8BEC(void) {
    if (D_800625A0->card->mode != 0) {
        if (++D_800625A0->cardPollTimer > D_801E9779) {
            func_801C8A10(0);
            func_801C8A10(1);
            if (D_800625A0->card->result[0] == -1 && D_800625A0->card->result[1] == -1) {
                D_800625A0->cardsPresent = 0;
            }
            D_800625A0->cardPollTimer = 0;
        }
    }
}

/* Unless port `port` could not be checked, show the card notice for 59 frames. */
void func_801C8CA4(u8 port) {
    MenuCard *card;
    s32 frames;

    card = D_800625A0->card;
    if (card->result[port] != -2) {
        card->mode = 2;
        frames = 59;
        do {
            frames--;
            func_801C7BF4();
        } while (frames != 0);
        D_800625A0->card->mode = 0;
    }
}

/* Unless port `port` could not be checked, wait 59 vertical blanks. */
void func_801C8D1C(u8 port) {
    s32 frames;

    if (D_800625A0->card->result[port] != -2) {
        frames = 59;
        do {
            VSync(0);
            frames--;
        } while (frames != 0);
    }
}

/* List the directory of card port `port`: clear the port's slot entries and
 * save marks, then copy each file name into the port's file entries.
 * Stores and returns the file count. */
u8 func_801C8D78(u8 port) {
    struct DIRENTRY dir;
    char device[8];
    s32 i;
    s32 retry;
    u8 count;

    D_801EA900[port] = 0;
    retry = 5;
    for (i = 0; i < 16; i++) {
        D_800625A0->card->fileSlots[port * 16 + i] = 0xff;
        D_801EA6D0[port][i] = 0;
    }
    if (port == 0) {
        __builtin_memcpy(device, D_801C50A8, 6);
    } else {
        __builtin_memcpy(device, D_801C50B0, 6);
    }
    while (--retry != 0) {
        count = 0;
        if (func_80040584(device, &dir) == &dir) {
            do {
                strcpy(D_800625A0->card->files[port * 16 + count].name, dir.name);
                count++;
            } while (func_80040594(&dir) == &dir);
        }
        D_800625A0->card->unk4F8A[port] = count;
        break;
    }
    return count;
}

/* List the files of each port not yet scanned; in mode 1 a port with files
 * marks the cards present. */
void func_801C8EE8(void) {
    switch (D_800625A0->card->mode) {
    case 1:
        if (D_800625A0->card->scanned[0] == 0) {
            if (func_801C8D78(0)) {
                D_800625A0->cardsPresent = 1;
            }
            D_800625A0->card->scanned[0] = 1;
        }
        if (D_800625A0->card->scanned[1] == 0) {
            if (func_801C8D78(1)) {
                D_800625A0->cardsPresent = 1;
            }
            D_800625A0->card->scanned[1] = 1;
        }
        break;
    case 2:
        if (D_800625A0->card->scanned[0] == 0) {
            func_801C8D78(0);
            D_800625A0->card->scanned[0] = 1;
        }
        if (D_800625A0->card->scanned[1] == 0) {
            func_801C8D78(1);
            D_800625A0->card->scanned[1] = 1;
        }
        break;
    }
}

/* Read the 512-byte first block of card file `name` into `dst`; 0 on success,
 * -1 on failure. */
s32 func_801C9038(char *name, void *dst) {
    s32 fd;

    fd = open(name, 3);
    if (fd == -1) {
        return -1;
    }
    if (read(fd, dst, 0x200) != 0x200) {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

/* Read the first block of file `file` on port `port` into its header buffer
 * and list the file's blocks in the port's slot entries. */
void func_801C90B0(u8 port, u8 file) {
    char device[8];
    char name[64];
    s32 retry;
    s32 i;

    retry = 1;
    if (port == 0) {
        __builtin_memcpy(device, D_801C50A8, 6);
    } else {
        __builtin_memcpy(device, D_801C50B0, 6);
    }
    strcpy(name, device);
    strcat(name, D_800625A0->card->files[port * 16 + file].name);
    while (func_801C9038(name, D_800625A0->card->headers[port * 16 + file]) == -1) {
        if (--retry == 0) {
            break;
        }
        func_801C8CA4(port);
    }
    for (i = 0; i < D_800625A0->card->headers[port * 16 + file][3]; i++) {
        D_800625A0->card->fileSlots[port * 16 + D_800625A0->card->fileCount] = file + port * 16;
        D_800625A0->card->fileCount++;
    }
}

/* Mark the files of `port` whose names carry this game's prefix and note the
 * save slot each holds. */
void func_801C9270(s32 port) {
    s32 i;
    s32 j;
    s32 match;
    u8 *header;
    u8 *saves;

    for (i = 0; i < 16; i++) {
        D_800625A0->card->ours[port * 16 + i] = 0;
    }
    for (i = 0; i < 15; i++) {
        j = 0;
        match = 1;
        for (; j < 12; j++) {
            if (D_800625A0->card->files[D_800625A0->card->fileSlots[port * 16 + i]].name[j] !=
                D_800625A0->card->prefix[j]) {
                match = 0;
                break;
            }
        }
        if (match) {
            D_800625A0->card->ours[port * 16 + i] = 1;
            header = D_800625A0->card->headers[D_800625A0->card->fileSlots[port * 16 + i]];
            D_801EA6F4 = header + 0x100;
            saves = D_801EA6D0[port];
            saves[header[0x123]] = 1;
        }
    }
}
