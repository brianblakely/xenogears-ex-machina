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
u8 D_801E96A6 = 8; /* unreferenced; a variable or stray fill (open, docs/matching.md) */
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
/* 801E1544 screen: five sheet images per row (ff none) for its 13 rows
 * (func_801E1AC8). Its alignment padding holds stray assembler bytes
 * (00 07 2e) that nothing reads, so it stays original data
 * (slot39.classification.txt). */
INCLUDE_ORIGINAL(".data", D_801E97AC, 0x801E97AC, 68);
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

/* The unit's uninitialized variables, zero in the file after all units'
 * initialized data, each in a slot of whole words (decomp/Makefile). */
static u8 D_801EA6D0[2][16]; /* per port and save slot: a save of this game exists */
static s32 D_801EA6F0;       /* unreferenced */
static u8 *D_801EA6F4;       /* the save information of the last matched file */
static u8 D_801EA6F8;
static s32 D_801EA6FC; /* gauge: from, to, difference and lengths */
static s32 D_801EA700;
static s32 D_801EA704;
static s32 D_801EA708;
static s32 D_801EA70C;
static u8 D_801EA710;
static u8 D_801EA714;
static s32 D_801EA718; /* card event and handler ids */
static s32 D_801EA71C;
static s32 D_801EA720;

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
    if (_card_info(channel) == 0) {
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
        __builtin_memcpy(device, "bu00:", 6);
    } else {
        __builtin_memcpy(device, "bu10:", 6);
    }
    while (--retry != 0) {
        count = 0;
        if (firstfile(device, &dir) == &dir) {
            do {
                strcpy(D_800625A0->card->files[port * 16 + count].name, dir.name);
                count++;
            } while (nextfile(&dir) == &dir);
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
        __builtin_memcpy(device, "bu00:", 6);
    } else {
        __builtin_memcpy(device, "bu10:", 6);
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

/* Refresh the cards before a save or load. With no card in either port: show
 * message 23 and, still none, wait a second and return 1; otherwise reset
 * the card events and listings. With cards: relist each port whose file
 * count changed (erase the temporary file, read each file's header and icon)
 * while the cards stay as they were, mark this game's files and place the
 * cursor marker. */
u8 func_801C93A8(void) {
    char path[64];
    u8 present[2];
    u8 unused[48]; /* unused in the original; reserves 48 bytes */
    MenuState *state;
    s32 port;
    s32 i;
    s32 wait;
    u8 noCard;
    u8 marked;
    u8 stop;

    D_801EA6F8 = 0;
    D_800625A0->card->mode = 2;
    func_801C7BF4();
    /* Always 0 (a byte shifted right by 8), but only combine sees that: both
     * cse passes keep `marked` a register copy (move s6,s4) and the cursor
     * marker code still tests it, so its store is never reached. */
    noCard = D_800625A0->card->present[0] >> 8;
    marked = noCard;
    if (*(u16 *)D_800625A0->card->present == 0) {
        func_801D32B4();
        D_800625A0->party->messageShown = 0;
        D_800625A0->party->unkB = 0;
        func_801C7BF4();
        func_801D2F4C(0x23);
        D_800625A0->loadState = 1;
        D_800625A0->party->unk2F = 0;
        D_800625A0->card->unk4F80 = 0xff;
        D_800625A0->card->cursor = 0;
        if (*(u16 *)D_800625A0->card->present == 0) {
            D_800625A0->card->unk4F8C[0] = D_800625A0->card->unk4F8C[1] = 0xff;
            func_801C7BF4();
            wait = 59;
            do {
                VSync(0);
            } while (--wait != 0);
            noCard = 1;
        }
        if (!noCard) {
            func_801D9B08();
            D_800625A0->card->presentShown[0] = D_800625A0->card->presentShown[1] = 0xff;
            D_800625A0->card->scanned[0] = 0;
            D_800625A0->card->scanned[1] = 0;
            D_800625A0->cardPollTimer = 0x3c;
            func_801C7BF4();
            D_801EA6F8 = 1;
        }
        func_801D32B4();
    }
    D_801E9779 = 1;
    present[0] = D_800625A0->card->present[0];
    present[1] = D_800625A0->card->present[1];
    if (!noCard) {
        D_800625A0->sounds = 0;
        for (port = 0; port < 2; port++) {
            stop = 0;
            if (D_800625A0->card->unk4F8A[port] != D_800625A0->card->unk4F8C[port] &&
                D_800625A0->card->unk4F8A[port] != 0) {
                func_801D9B08();
                for (i = 0; i < 16; i++) {
                    D_800625A0->card->fileSlots[port * 16 + i] = 0xff;
                }
                if (port == 0) {
                    __builtin_memcpy(path, "bu00:", 6);
                } else {
                    __builtin_memcpy(path, "bu10:", 6);
                }
                strcat(path, "__tmp_file");
                delete(path);
                D_800625A0->card->fileCount = 0;
                D_801EA900[port] = 0;
                for (i = 0; i < D_800625A0->card->unk4F8A[port]; i++) {
                    if (D_800625A0->card->present[port] != 0 && present[0] == D_800625A0->card->present[0] &&
                        present[1] == D_800625A0->card->present[1]) {
                        func_801C90B0(port, i);
                        func_801E78C8(port * 16 + i);
                        D_800625A0->card->files[port * 16 + i].state = 1;
                        func_801C7BF4();
                        if (D_800625A0->card->scanned[port] != 0) {
                            continue;
                        }
                        stop = port + 1;
                    } else {
                        stop = 2;
                    }
                    port = 2;
                    noCard = 0;
                    break;
                }
                if (!stop) {
                    for (; i < 16; i++) {
                        D_800625A0->card->files[port * 16 + i].state = 0;
                    }
                    D_800625A0->card->unk4F8C[port] = D_800625A0->card->unk4F8A[port];
                }
            }
            if (!stop && D_800625A0->card->unk4F8A[port] == 0) {
                for (i = 0; i < 16; i++) {
                    D_800625A0->card->files[port * 16 + i].state = 0;
                }
                D_800625A0->card->unk4F8C[port] = 0xff;
            }
        }
        if (!stop) {
            func_801C9270(0);
            func_801C9270(1);
        }
    }
    if (!stop) {
        if (marked) {
            D_800625A0->party->unkB = 1;
        }
        state = D_800625A0;
        if (state->party->unk2F != 0 && state->markers->unk144[0] != 0) {
            setXY4(&state->markers->polys[state->markers->current[0]],
                   D_801E9894[D_801E981C[state->card->cursor]][0] + 8, D_801E9914[D_801E981C[state->card->cursor]][0] - 6,
                   D_801E9894[D_801E981C[state->card->cursor]][0] + 0x18, D_801E9914[D_801E981C[state->card->cursor]][0] - 6,
                   D_801E9894[D_801E981C[state->card->cursor]][0] + 8, D_801E9914[D_801E981C[state->card->cursor]][0] + 0xa,
                   D_801E9894[D_801E981C[state->card->cursor]][0] + 0x18, D_801E9914[D_801E981C[state->card->cursor]][0] + 0xa);
        }
    }
    D_800625A0->sounds = 1;
    D_801E9779 = 0x1e;
    return noCard;
}

/* Whether the cursor slot suits `mode`: 0 a file is there, 1 this game's file
 * is there (both need files listed), 2 its card is present; other modes always
 * suit. A suitable slot sets the load state to 2. */
s32 func_801C9BCC(s32 mode) {
    MenuCard *card;
    s32 cursor;
    s32 ok;

    card = D_800625A0->card;
    ok = 1;
    cursor = card->cursor;
    switch (mode) {
    case 0:
        if (*(u32 *)card->scanned & 0xffff0000) {
            if (card->present[D_801E981C[cursor] / 16]) {
                if (card->fileSlots[D_801E981C[cursor]] == 0xff) {
                    ok = 0;
                }
            } else {
                ok = 0;
            }
        } else {
            ok = 0;
        }
        break;
    case 1:
        if (*(u32 *)card->scanned & 0xffff0000) {
            if (card->present[D_801E981C[cursor] / 16] && card->fileSlots[D_801E981C[cursor]] != 0xff) {
                if (!card->ours[D_801E981C[cursor]]) {
                    ok = 0;
                }
            } else {
                ok = 0;
            }
        } else {
            ok = 0;
        }
        break;
    case 2:
        if (!card->present[D_801E981C[cursor] / 16]) {
            ok = 0;
        }
        break;
    }
    if (ok) {
        D_800625A0->loadState = 2;
    }
    return ok;
}

/* The first cursor position of the present cards whose slot suits `mode`
 * (as 801c9bcc), or ff; modes 0 and 1 stop at once when no files are listed.
 * Sets the load state to 2. */
s32 func_801C9D34(s32 mode) {
    s32 found;
    s32 searching;
    s32 end;
    s32 first;
    s32 i;

    found = 0xff;
    searching = 1;
    end = 30;
    first = D_800625A0->card->present[0] == 0 ? 15 : 0;
    if (D_800625A0->card->present[1] == 0) {
        end = 15;
    }
    for (i = first; i < end && searching; i++) {
        switch (mode) {
        case 0:
            if (*(u32 *)D_800625A0->card->scanned & 0xffff0000) {
                if (D_800625A0->card->present[D_801E981C[i] / 16] &&
                    D_800625A0->card->fileSlots[D_801E981C[i]] != 0xff) {
                    found = i;
                    searching = 0;
                }
            } else {
                searching = 0;
            }
            break;
        case 1:
            if (*(u32 *)D_800625A0->card->scanned & 0xffff0000) {
                if (D_800625A0->card->present[D_801E981C[i] / 16] &&
                    D_800625A0->card->fileSlots[D_801E981C[i]] != 0xff && D_800625A0->card->ours[D_801E981C[i]]) {
                    found = i;
                    searching = 0;
                }
            } else {
                searching = 0;
            }
            break;
        case 2:
            if (D_800625A0->card->present[D_801E981C[i] / 16]) {
                found = i;
                searching = 0;
            }
            break;
        }
    }
    D_800625A0->loadState = 2;
    return found;
}

/* Move the cursor from `slot` a row down for `mode`: 0 to the next slot
 * below holding a file, else to the first file after the next row; 1 the
 * same for this game's files; 2 a row down when that card is present.
 * Mode 2 keeps the shape of the other modes' scans as single-pass loops. */
void func_801C9EF4(s32 mode, s32 slot) {
    s32 i;

    switch (mode) {
    case 0:
        for (; slot + 3 < 30; slot += 3) {
            if (D_800625A0->card->fileSlots[D_801E981C[slot + 3]] != 0xff) {
                D_800625A0->card->cursor = slot + 3;
                break;
            }
        }
        if (slot + 3 >= 30) {
            i = D_800625A0->card->cursor + 3;
            if (i < 30) {
                for (; i + 1 < 30; i++) {
                    if (D_800625A0->card->fileSlots[D_801E981C[i + 1]] != 0xff) {
                        D_800625A0->card->cursor = i + 1;
                        break;
                    }
                }
            }
        }
        break;
    case 1:
        while (slot + 3 < 30) {
            if (D_800625A0->card->fileSlots[D_801E981C[slot + 3]] == 0xff ||
                !D_800625A0->card->ours[D_801E981C[slot + 3]]) {
                slot += 3;
            } else {
                D_800625A0->card->cursor = slot + 3;
                break;
            }
        }
        if (slot + 3 >= 30) {
            i = D_800625A0->card->cursor + 3;
            if (i < 30) {
                while (i + 1 < 30) {
                    if (D_800625A0->card->fileSlots[D_801E981C[i + 1]] == 0xff ||
                        !D_800625A0->card->ours[D_801E981C[i + 1]]) {
                        i++;
                    } else {
                        D_800625A0->card->cursor = i + 1;
                        break;
                    }
                }
            }
        }
        break;
    case 2:
        if (D_800625A0->card->present[(slot + 3) / 15]) {
            for (; slot + 3 < 30; slot += 3) {
                D_800625A0->card->cursor = slot + 3;
                break;
            }
            if (slot + 3 >= 30) {
                i = D_800625A0->card->cursor + 3;
                if (i < 30) {
                    for (; i + 1 < 30; i++) {
                        D_800625A0->card->cursor = i + 1;
                        break;
                    }
                }
            }
        }
        break;
    }
}

/* Move the cursor from `slot` a row up for `mode`: 0 to the next slot above
 * holding a file, else to the last file before the previous row; 1 the same
 * for this game's files; 2 a row up when that card is present. */
void func_801CA1D4(s32 mode, s32 slot) {
    s32 cursor;
    s32 i;

    switch (mode) {
    case 0:
        for (; slot - 3 >= 0; slot -= 3) {
            if (D_800625A0->card->fileSlots[D_801E981C[slot - 3]] != 0xff) {
                D_800625A0->card->cursor = slot - 3;
                break;
            }
        }
        if (slot - 3 < 0) {
            i = D_800625A0->card->cursor - 3;
            if (i >= 0) {
                for (; i - 1 >= 0; i--) {
                    if (D_800625A0->card->fileSlots[D_801E981C[i - 1]] != 0xff) {
                        D_800625A0->card->cursor = i - 1;
                        break;
                    }
                }
            }
        }
        break;
    case 1:
        while (slot - 3 >= 0) {
            if (D_800625A0->card->fileSlots[D_801E981C[slot - 3]] == 0xff ||
                !D_800625A0->card->ours[D_801E981C[slot - 3]]) {
                slot -= 3;
            } else {
                D_800625A0->card->cursor = slot - 3;
                break;
            }
        }
        if (slot - 3 < 0) {
            cursor = D_800625A0->card->cursor;
            i = cursor - 3;
            if (i >= 0) {
                while (i - 1 >= 0) {
                    if (D_800625A0->card->fileSlots[D_801E981C[i - 1]] == 0xff ||
                        !D_800625A0->card->ours[D_801E981C[i - 1]]) {
                        i--;
                    } else {
                        D_800625A0->card->cursor = i - 1;
                        break;
                    }
                }
            }
        }
        break;
    case 2:
        if (D_800625A0->card->present[(slot - 3) / 15]) {
            for (; slot - 3 >= 0; slot -= 3) {
                D_800625A0->card->cursor = slot - 3;
                break;
            }
            if (slot - 3 < 0) {
                i = D_800625A0->card->cursor - 3;
                if (i >= 0) {
                    for (; i - 1 >= 0; i--) {
                        D_800625A0->card->cursor = i - 1;
                        break;
                    }
                }
            }
        }
        break;
    }
}

/* Move the cursor from `slot` right for `mode`: 0 to the next slot holding
 * a file, 1 to the next slot holding this game's file, 2 one slot on when
 * that card is present. */
void func_801CA480(s32 mode, s32 slot) {
    s32 next;

    switch (mode) {
    case 0:
        for (; slot + 1 < 30; slot++) {
            if (D_800625A0->card->fileSlots[D_801E981C[slot + 1]] != 0xff) {
                D_800625A0->card->cursor = slot + 1;
                break;
            }
        }
        break;
    case 1:
        while (slot + 1 < 30) {
            if (D_800625A0->card->fileSlots[D_801E981C[slot + 1]] == 0xff || !D_800625A0->card->ours[D_801E981C[slot + 1]]) {
                slot++;
            } else {
                D_800625A0->card->cursor = slot + 1;
                break;
            }
        }
        break;
    case 2:
        next = slot + 1;
        if (D_800625A0->card->present[next / 15] && next < 30) {
            D_800625A0->card->cursor = next;
        }
        break;
    }
}

/* Move the cursor from `slot` left for `mode`: 0 to the previous slot
 * holding a file, 1 to the previous slot holding this game's file, 2 one slot
 * back when that card is present. */
void func_801CA5F0(s32 mode, s32 slot) {
    s32 next;

    switch (mode) {
    case 0:
        for (; slot - 1 >= 0; slot--) {
            if (D_800625A0->card->fileSlots[D_801E981C[slot - 1]] != 0xff) {
                D_800625A0->card->cursor = slot - 1;
                break;
            }
        }
        break;
    case 1:
        while (slot - 1 >= 0) {
            if (D_800625A0->card->fileSlots[D_801E981C[slot - 1]] == 0xff || !D_800625A0->card->ours[D_801E981C[slot - 1]]) {
                slot--;
            } else {
                D_800625A0->card->cursor = slot - 1;
                break;
            }
        }
        break;
    case 2:
        next = slot - 1;
        if (D_800625A0->card->present[next / 15] && next >= 0) {
            D_800625A0->card->cursor = next;
        }
        break;
    }
}

/* Unreferenced; it follows the card file strings, which are literals. */
const char D_801C50C4[] = "";

/* The file screen's cursor input for `mode` (801c9bcc): move the cursor
 * (down, right, up, left), confirm (1) or cancel (2); a new cursor slot shows
 * its file's details. */
s32 func_801CA750(s32 mode) {
    s32 result;

    result = 0;
    switch (D_800625A0->input) {
    case 4:
        result = 1;
        break;
    case 5:
        result = 2;
        break;
    case 0:
        func_801C9EF4(mode, D_800625A0->card->cursor);
        break;
    case 2:
        func_801CA1D4(mode, D_800625A0->card->cursor);
        break;
    case 1:
        func_801CA480(mode, D_800625A0->card->cursor);
        break;
    case 3:
        func_801CA5F0(mode, D_800625A0->card->cursor);
        break;
    }
    if (D_800625A0->card->cursor != D_800625A0->card->unk4F80) {
        func_801E781C(D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]],
                      D_800625A0->card->ours[D_801E981C[D_800625A0->card->cursor]]);
        D_800625A0->card->unk4F80 = D_800625A0->card->cursor;
    }
    return result;
}

/* Build the save header's title: "XENOGEARS/No." (full width), file number
 * `file` + 1 as two full-width digits, a full-width space and the save title
 * line. */
void func_801CA8C0(u8 file) {
    strcpy(D_800625A0->card->saveTitle, "\x82\x77\x82\x64\x82\x6d\x82\x6e\x82\x66\x82\x64\x82\x60\x82\x71\x82\x72"
                                         "\x81\x5e\x82\x6d\x82\x8f\x81\x44");
    D_800625A0->card->saveTitle[26] = 0x82;
    D_800625A0->card->saveTitle[27] = (file + 1) / 10 + 0x4f;
    D_800625A0->card->saveTitle[28] = 0x82;
    D_800625A0->card->saveTitle[29] = (file + 1) % 10 + 0x4f;
    D_800625A0->card->saveTitle[30] = 0x81;
    D_800625A0->card->saveTitle[31] = 0x40;
    D_800625A0->card->saveTitle[32] = 0;
    D_800625A0->card->saveTitle[33] = 0;
    strcat(D_800625A0->card->saveTitle, D_800625A0->card->title);
}

/* The yes/no choice: wait for confirm or cancel while left/right move the
 * highlight (2 yes, 0 no). Without `watch` it waits b4h frames at most while
 * no input comes and returns on any other input; with `watch` a card change
 * clears that port's listing and cancels. Returns 1 for yes. */
u8 func_801CAA38(u8 watch) {
    u8 present[2];
    u8 yes;
    u8 waiting;
    u8 timer;

    present[0] = D_800625A0->card->present[0];
    present[1] = D_800625A0->card->present[1];
    waiting = 1;
    if (D_801E977A) {
        D_800625A0->card->mode = 2;
    }
    D_801EA8FC = 0;
    yes = 0;
    timer = 0xb4;
    do {
        if (!watch) {
            D_800625A0->markers->visible[2] = 0;
            D_800625A0->markers->visible[3] = 0;
            if (D_800625A0->input != 8 || --timer == 0) {
                break;
            }
        }
        func_801C7BF4();
        if (watch && D_801E977A) {
            if (present[0] != D_800625A0->card->present[0]) {
                D_800625A0->card->scanned[0] = 0;
                D_800625A0->input = 5;
                D_801E9778 = 1;
            }
            if (present[1] != D_800625A0->card->present[1]) {
                D_800625A0->card->scanned[1] = 0;
                D_800625A0->input = 5;
                D_801E9778 = 1;
            }
        }
        switch (D_800625A0->input) {
        case 4:
            waiting = 0;
            break;
        case 5:
            waiting = yes = 0;
            D_801EA8FC = 1;
            break;
        case 2:
            D_800625A0->markers->visible[2] = 1;
            yes = 1;
            D_800625A0->markers->visible[3] = 0;
            break;
        case 0:
            D_800625A0->markers->visible[2] = 0;
            yes = 0;
            D_800625A0->markers->visible[3] = 1;
            break;
        }
    } while (waiting);
    D_800625A0->markers->visible[2] = 0;
    D_800625A0->markers->visible[3] = 0;
    D_800625A0->card->mode = 0;
    return yes;
}

/* Show card message `message` and ask (801caa38 with `arg`); when answered
 * yes and a follow-up `confirm` is given (not ff), ask that too. Returns the
 * last answer. */
s32 func_801CACF8(u8 message, u8 confirm, u8 arg) {
    u8 answer;

    func_801D2F4C(message);
    D_800625A0->markers->visible[3] = 1;
    answer = func_801CAA38(arg);
    func_801D32B4();
    if (confirm != 0xff && answer) {
        func_801D2F4C(confirm);
        D_800625A0->markers->visible[3] = 1;
        answer = func_801CAA38(arg);
        func_801D32B4();
    }
    return answer;
}

/* Reset the file cursor: the first port with a card (port 2's slots start at 15). */
void func_801CADB0(void) {
    D_800625A0->card->unk4F80 = 0xff;
    D_800625A0->card->cursor = 0;
    if (D_800625A0->card->present[0] == 0 && D_800625A0->card->present[1] != 0) {
        D_800625A0->card->cursor = 15;
    }
}

/* The card access indicator: 0 removes it (after a frame its block is
 * released), 1 builds it (a flat quad and sprites 160/161 at (a0, 64)) and
 * starts its effects, 2 closes it. */
void func_801CAE08(u8 mode) {
    switch (mode) {
    case 0:
        if (D_800625A0->party->unk50[2]) {
            D_800625A0->party->unk50[2] = 0;
            func_801C7BF4();
            func_800320E8(MENU_INDICATOR);
        }
        break;
    case 1:
        D_800625A0->blocks444[2] = func_80031BDC(sizeof(MenuIndicator), 0);
        bzero(MENU_INDICATOR, sizeof(MenuIndicator));
        SetPolyF4(&MENU_INDICATOR->fills[D_800625A0->bufferIndex]);
        setRGB0(&MENU_INDICATOR->fills[D_800625A0->bufferIndex], 0xa0, 0xa0, 0);
        func_8002675C(D_800625A0->sheet, 0x160, MENU_INDICATOR->spriteA, D_800625A0->bufferIndex, 0xa0, 0x64, 0x1000);
        func_8002675C(D_800625A0->sheet, 0x161, MENU_INDICATOR->spriteB, D_800625A0->bufferIndex, 0xa0, 0x64, 0x1000);
        MENU_INDICATOR->buffer = D_800625A0->bufferIndex;
        D_800625A0->party->unk50[2] = 1;
        MENU_INDICATOR->unk7B4 = 8;
        func_80039E60((D_800625A0->effectBank->id << 16) | 0xe0);
        func_80039E60((D_800625A0->effectBank->id << 16) | 0xe1);
        func_80039E60((D_800625A0->effectBank->id << 16) | 0x8f);
        break;
    case 2:
        if (D_800625A0->party->unk50[2]) {
            MENU_INDICATOR->unk7B0 = 0x100;
            setRGB0(&MENU_INDICATOR->fills[MENU_INDICATOR->buffer], 0, 0xa0, 0);
            D_800625A0->party->unk50[2] = 2;
        }
        break;
    }
}

/* Decode the 31 names of the game data in place: each name's code pairs up
 * to the first zero pair go through 80033b34 into a 20-byte buffer that is
 * copied back whole. */
void func_801CB184(void) {
    u8 codes[24];
    u8 decoded[20];
    s32 n;
    s32 i;
    u8 *src;
    u8 *dst;

    for (n = 0; n < 31; n++) {
        for (i = 0; i < 20; i += 2) {
            src = &GAME_NAMES[n * 20];
            codes[i] = src[i];
            codes[i + 1] = GAME_NAMES[n * 20 + i + 1];
            if (src[i] == 0 && GAME_NAMES[n * 20 + i + 1] == 0) {
                break;
            }
        }
        func_80033B34(codes, decoded, i / 2);
        dst = &GAME_NAMES[n * 20];
        for (i = 0; i < 20; i++) {
            dst[i] = decoded[i];
        }
    }
}

/* Apply a loaded save: its derived tables, play time and the 16 resident
 * words copied from the game data, then finish (801cb184). */
void func_801CB28C(s32 *save) {
    s32 i;

    func_801E4D10(save, D_800625A0->tables);
    D_80059488 = *save;
    for (i = 0; i < 16; i++) {
        D_8005A3A0[i] = D_8006F958[i];
    }
    func_801CB184();
}

/* The load: refresh the cards (no card returns), find a slot with this
 * game's file (none: message 62 and return 0), then run the cursor until
 * cancel (return 0) or confirm: ask (message 65); on yes read the file in
 * 100h chunks into a 2100h block and, when the payload's eight-bit sum matches
 * its last byte, apply it; message 5c ends the load, a failure shows message
 * 3e and continues. */
/* Read the save file in 100h chunks into a 2100h block (a failed read
 * closes the file and frees the block) and apply it when its sum
 * matches (a statement macro). */
#define READ_SAVE()                                          \
    do {                                                     \
        D_800625A0->party->unk2F = 0;                        \
        buffer = func_80031BDC(0x2100, 1);                   \
        done = 0;                                            \
        p = buffer;                                          \
        D_800625A0->party->unkB = 0;                         \
        func_801CAE08(1);                                    \
        do {                                                 \
            func_801C7BF4();                                 \
            retry = 5;                                       \
            do {                                             \
                if (read(fd, p, 0x100) != 0x100) {           \
                    fd = 0;                                  \
                    func_801C8CA4(port);                     \
                }                                            \
            } while (fd == 0 && --retry != 0);               \
            if (fd == 0) {                                   \
                close(file);                                 \
                goto release;                                \
            }                                                \
            done += 0x100;                                   \
            p += 0x100;                                      \
        } while (done < D_800625A0->card->saveBlocks << 13); \
        close(fd);                                           \
        p = buffer + 0x100;                                  \
        sum = 0;                                             \
        for (i = 0; i < 0x1eff; i++) {                       \
            sum += *p++;                                     \
        }                                                    \
        if (sum == *p) {                                     \
            p = buffer + 0x100;                              \
            func_801C72BC(1);                                \
            func_801CB28C((s32 *)p);                         \
            func_801C72BC(0x11);                             \
        } else {                                             \
            fd = 0;                                          \
        }                                                    \
    } while (0)

u8 func_801CB304(void) {
    char path[64];
    s32 first;
    u8 result;
    s32 again;
    u8 port;
    u8 retry;
    s32 fd;
    s32 file;
    s32 done;
    u8 *buffer;
    u8 *p;
    u8 sum;
    s32 i;
    s32 failed; /* open's failure result, held in a variable */

    result = 1;
    first = 1;
    again = 1;
    func_801CADB0();
    do {
        if (func_801C93A8()) {
            break;
        }
        if (first) {
            if (D_800625A0->party->messageShown) {
                func_801D32B4();
            }
            first = 0;
            D_800625A0->markers->visible[0] = 1;
        }
        if (!func_801C9BCC(1) && (D_800625A0->card->cursor = func_801C9D34(1)) == 0xff) {
            D_800625A0->party->unk2F = 0;
            D_800625A0->party->unkB = 0;
            D_800625A0->loadState = 1;
            D_800625A0->party->unkB = 0;
            func_801CACF8(0x62, 0xff, 0);
            result = 0;
            break;
        }
        D_800625A0->party->unk2F = 1;
        switch (func_801CA750(1)) {
        case 1:
            D_800625A0->markers->unk144[0] = 0;
            D_800625A0->card->mode = 0;
            port = 0;
            if (D_800625A0->card->cursor < 15) {
                __builtin_memcpy(path, "bu00:", 6);
            } else {
                __builtin_memcpy(path, "bu10:", 6);
                port = 1;
            }
            strcat(path,
                   D_800625A0->card->files[D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]]].name);
            if (!(u8)func_801CACF8(0x65, 0xff, 1)) {
                D_800625A0->card->unk4F80 = 0xff;
            } else {
                func_801D2F4C(0x3b);
                retry = 5;
                D_800625A0->sounds = 0;
                failed = -1;
                do {
                    fd = open(path, 1);
                    if (fd == failed) {
                        fd = 0;
                        func_801C8CA4(port);
                    }
                } while (fd == 0 && --retry != 0);
                file = fd;
                if (file != 0) {
                    READ_SAVE();
                release:
                    func_800320E8(buffer);
                }
                func_801CAE08(2);
                func_801D32B4();
                if (fd != 0) {
                    again = 0;
                    D_800625A0->sounds = 1;
                    func_801C8574(0x34);
                    D_800625A0->sounds = 0;
                    func_801CACF8(0x5c, 0xff, 0);
                } else {
                    func_801CACF8(0x3e, 0xff, 0);
                    D_800625A0->card->mode = 2;
                }
                func_801CAE08(0);
                D_800625A0->card->scanned[0] = 0;
                D_800625A0->card->scanned[1] = 0;
                D_800625A0->card->unk4F8C[0] = 0xff;
                D_800625A0->card->unk4F8C[1] = 0xff;
            }
            D_800625A0->markers->unk144[0] = 1;
            break;
        case 2:
            again = 0;
            result = 0;
            D_800625A0->sounds = 1;
            break;
        }
    } while (again);
    D_800625A0->card->mode = 1;
    D_800625A0->cardsPresent = 1;
    return result;
}

/* The unformatted-card question for `port`: show message 29h + 3 * port and
 * wait while no input comes and the cards stay as they were; a card change
 * returns 0, otherwise the answer to message 2f. */
u8 func_801CB8AC(u8 port) {
    u8 present[2];
    u8 changed;
    u8 answer;

    present[0] = D_800625A0->card->present[0];
    present[1] = D_800625A0->card->present[1];
    func_801D2F4C(port * 3 + 0x29);
    D_800625A0->input = 8;
    answer = 0;
    D_800625A0->card->mode = 2;
    changed = 0;
    while (D_800625A0->input == 8) {
        func_801C7BF4();
        if (present[0] != D_800625A0->card->present[0] || present[1] != D_800625A0->card->present[1]) {
            changed = 1;
            break;
        }
    }
    func_801D32B4();
    if (!changed) {
        answer = func_801CACF8(0x2f, 0xff, 1);
    }
    return answer;
}

/* The save slot to use on `port`: `slot`, or for ff the first free one. */
u8 func_801CB9E8(u8 port, u8 slot) {
    s32 i;
    u8 found;

    found = 0;
    if (slot == 0xff) {
        for (i = 0; i < 15; i++) {
            if (D_801EA6D0[port][i] == 0) {
                found = i;
                break;
            }
        }
        return found;
    }
    return slot;
}

/* Fill the zeroed save payload: the globals 8005a3a0 into the game data, the
 * party summary (per slot: character id or ff, HP, maximum HP, EP, maximum EP
 * and three more bytes), the play time and file digit `digit`, each name
 * encoded in place, the disc, then the game data copy (801e4a28) and the names
 * decoded back. */
void func_801CBA4C(MenuSavePayload *payload, u8 port, u8 digit) {
    u8 codes[24];
    u8 encoded[20];
    s32 i;
    s32 j;
    u8 *name;
    u8 *names;

    for (j = 0; j < 16; j++) {
        D_8006F958[j] = D_8005A3A0[j];
    }
    for (i = 0; i < 3; i++) {
        if (D_800625A0->party->ids[i] != 0xff) {
            payload->ids[i] = D_800625A0->party->ids[i];
            payload->hp[i] = D_8006D8A0[D_800625A0->party->ids[i]].hp;
            payload->hpMax[i] = D_8006D8A0[D_800625A0->party->ids[i]].hpMax;
            payload->ep[i] = D_8006D8A0[D_800625A0->party->ids[i]].ep;
            payload->epMax[i] = D_8006D8A0[D_800625A0->party->ids[i]].epMax;
            payload->unk16[i] = D_8006D8A0[D_800625A0->party->ids[i]].unk62;
            payload->unk19[i] = D_8006D8A0[D_800625A0->party->ids[i]].unk63;
        } else {
            payload->ids[i] = 0xff;
        }
    }
    payload->time = D_80059488;
    payload->unk1F = 0;
    payload->digit = digit;
    names = GAME_NAMES;
    for (i = 0; i < 31; i++) {
        name = &names[i * 20];
        for (j = 0; j < 20; j++) {
            codes[j] = name[j];
            encoded[j] = 0;
        }
        func_80033C20(codes, encoded);
        for (j = 0; j < 20; j++) {
            name[j] = encoded[j];
        }
    }
    if (!D_801E96A5) {
        D_8006F008 = func_80028530() - 1;
    } else {
        D_8006F008 = 1;
    }
    func_801E4A28((SaveData *)payload);
    func_801CB184();
}

/* The save: refresh the cards (none: returns 1), then run the file cursor
 * until cancel or confirm. An unformatted card asks to format it; an empty
 * slot asks message 5f, another game's file is refused (c4) and this game's
 * file asks 38 and is erased (keeping its digit). The save writes the
 * header (title 801ca8c0) and the checksummed payload (801cba4c) to a
 * temporary file and renames it to the prefix and digit, then reports 5c or
 * 35 and refreshes the listing. With `kind` 0 one save ends it. */
u8 func_801CBD90(u8 kind) {
    char finalName[64];
    char tempName[64];
    char device[8];
    char digitText[16];
    s32 again;
    s32 created;
    u8 first;
    u8 result;
    u8 existing;
    u8 proceed;
    u8 port;
    u8 digit;
    u8 retry;
    s32 fd;
    s32 written;
    u8 *buffer;
    u8 *p;
    u8 *q;
    u8 sum;
    s32 i;

    existing = 0xff;
    first = 1;
    again = 1;
    result = 0;
    func_801CADB0();
    do {
        if (func_801C93A8()) {
            result = 1;
            break;
        }
        if (first) {
            if (D_800625A0->party->messageShown) {
                func_801D32B4();
            }
            first = 0;
            D_800625A0->markers->visible[0] = 1;
        }
        if (!func_801C9BCC(2) && (D_800625A0->card->cursor = func_801C9D34(2)) == 0xff) {
            func_801CACF8(0xac, 0xff, 0);
            break;
        }
        D_800625A0->party->unk2F = 1;
        switch (func_801CA750(2)) {
        case 1:
            D_800625A0->markers->unk144[0] = 0;
            D_800625A0->card->mode = 0;
            proceed = 1;
            if (D_800625A0->card->cursor < 15) {
                __builtin_memcpy(device, "bu00:", 6);
                port = 0;
            } else {
                __builtin_memcpy(device, "bu10:", 6);
                port = 1;
            }
            if (D_800625A0->card->result[port] == -2) {
                if (!func_801CB8AC(port)) {
                    D_800625A0->markers->unk144[0] = 1;
                    continue;
                }
                func_801D2F4C(0x26);
                if (format(device)) {
                    func_801D32B4();
                    func_801CACF8(0x5c, 0xff, 0);
                } else {
                    func_801D32B4();
                    proceed = 0;
                    again = 0;
                }
            }
            if (proceed) {
                if (D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]] != 0xff) {
                    if (D_800625A0->card->ours[D_801E981C[D_800625A0->card->cursor]]) {
                        if ((u8)func_801CACF8(0x38, 0xff, 1)) {
                            strcpy(tempName, device);
                            strcat(tempName, D_800625A0->card
                                                 ->files[D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]]]
                                                 .name);
                            delete(tempName);
                            existing = (D_800625A0->card->headers +
                                        D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]])[0][0x123];
                        } else {
                            proceed = 0;
                            D_800625A0->card->unk4F80 = 0xff;
                        }
                    } else {
                        func_801CACF8(0xc4, 0xff, 0);
                        D_800625A0->markers->unk144[0] = 1;
                        continue;
                    }
                } else if (!(u8)func_801CACF8(0x5f, 0xff, 1)) {
                    proceed = 0;
                    D_800625A0->card->unk4F80 = 0xff;
                }
            }
            if (proceed) {
                D_800625A0->party->unk2F = 0;
                D_800625A0->party->unkB = 0;
                D_800625A0->card->unk4F80 = 0xff;
                D_800625A0->card->unk4F8C[0] = 0xff;
                D_800625A0->card->unk4F8C[1] = 0xff;
                digit = func_801CB9E8(port, existing);
                digitText[0] = digit + '0';
                digitText[1] = 0;
                strcpy(finalName, device);
                strcpy(tempName, device);
                strcat(finalName, D_800625A0->card->prefix);
                strcat(finalName, digitText);
                strcat(tempName, "__tmp_file");
                func_801D2F4C(0x32);
                D_800625A0->sounds = 0;
                delete(tempName);
                retry = 3;
                do {
                    fd = open(tempName, D_800625A0->card->saveBlocks << 16 | 0x200);
                    if (fd == -1) {
                        fd = 0;
                        func_801C8CA4(port);
                    } else {
                        created = fd;
                    }
                } while (fd == 0 && --retry != 0);
                close(created);
                if (fd != 0) {
                    retry = 3;
                    do {
                        fd = open(tempName, 2);
                        if (fd == -1) {
                            fd = 0;
                            func_801C8CA4(port);
                        }
                    } while (fd == 0 && --retry != 0);
                    if (fd == 0) {
                        delete(tempName);
                    }
                    func_801C7BF4();
                }
                if (fd != 0) {
                    func_801CA8C0(digit);
                    retry = 3;
                    do {
                        if (write(fd, D_800625A0->card->saveMagic, 0x100) == -1) {
                            fd = 0;
                            func_801C8CA4(port);
                        }
                    } while (fd == 0 && --retry != 0);
                    if (fd == 0) {
                        close(0);
                        delete(tempName);
                    } else {
                        written = 0x100;
                        buffer = func_80031BDC(0x1f00, 1);
                        bzero(buffer, 0x1f00);
                        p = buffer;
                        func_801CBA4C((MenuSavePayload *)buffer, port, digit);
                        q = buffer;
                        sum = 0;
                        for (i = 0; i < 0x1eff; i++) {
                            sum += *q++;
                        }
                        *q = sum;
                        func_801CAE08(1);
                        do {
                            func_801C7BF4();
                            retry = 3;
                            do {
                                if (write(fd, p, 0x100) != 0x100) {
                                    fd = 0;
                                    func_801C8CA4(port);
                                }
                            } while (fd == 0 && --retry != 0);
                            if (fd == 0) {
                                close(created);
                                strcpy(tempName, device);
                                strcat(tempName, "__tmp_file");
                                delete(tempName);
                                goto release;
                            }
                            written += 0x100;
                            p += 0x100;
                        } while (written < D_800625A0->card->saveBlocks << 13);
                        close(fd);
                        strcpy(tempName, device);
                        strcat(tempName, "__tmp_file");
                        retry = 3;
                        do {
                            if (!rename(tempName, finalName)) {
                                fd = 0;
                                func_801C8CA4(port);
                            }
                        } while (fd == 0 && --retry != 0);
                        if (fd == 0) {
                            delete(tempName);
                        }
                    release:
                        func_800320E8(buffer);
                    }
                }
                func_801D32B4();
                func_801CAE08(2);
                if (fd != 0) {
                    D_800625A0->sounds = 1;
                    func_801C8574(0x34);
                    D_800625A0->sounds = 0;
                    func_801CACF8(0x5c, 0xff, 0);
                } else {
                    func_801CACF8(0x35, 0xff, 0);
                }
                func_801CAE08(0);
                D_800625A0->card->mode = 2;
                while (D_800625A0->cardPollTimer != 1) {
                    func_801C7BF4();
                }
                D_800625A0->card->scanned[0] = 0;
                D_800625A0->card->scanned[1] = 0;
            }
            D_800625A0->markers->unk144[0] = 1;
            D_800625A0->sounds = 1;
            if (kind) {
                break;
            }
            /* fall through: a single save ends it */
        case 2:
            again = 0;
            break;
        }
    } while (again);
    D_800625A0->card->mode = 1;
    D_800625A0->cardsPresent = 1;
    return result;
}

/* The file screen's copy command: pick a file and copy it to the other
 * card (asking first; no card, a full card or an existing file there
 * refuse). The copy reads the header and data blocks and writes them to a
 * temporary file that is then renamed; it reports 5c, or ac for a full
 * card and 44 otherwise, and refreshes the listing. Returns 1 when there
 * was no card. */
u8 func_801CC6D8(void) {
    char destName[64];
    char other[8];
    char srcName[64];
    char tempName[64];
    char device[8];
    u8 buffer[0x200];
    u8 header[0x200];
    s32 src;
    u8 first;
    u8 result;
    u8 full;
    s32 again;
    u8 proceed;
    s32 dest;
    u8 ask;
    u8 noCard;
    u8 exists;
    u8 retry;
    s32 phase;
    s32 fd;
    s32 srcFd;
    s32 destFd;
    s32 written;
    s32 i;
    s32 slot;

    full = 0;
    first = 1;
    again = 1;
    result = 0;
    func_801CADB0();
    do {
        if (func_801C93A8()) {
            result = 1;
            break;
        }
        if (first) {
            if (D_800625A0->party->messageShown) {
                func_801D32B4();
            }
            first = 0;
            D_800625A0->markers->visible[0] = 1;
        }
        if (!func_801C9BCC(0) && (D_800625A0->card->cursor = func_801C9D34(0)) == 0xff) {
            D_800625A0->markers->visible[0] = 0;
            D_800625A0->party->unkB = 0;
            func_801CACF8(0x62, 0xff, 0);
            break;
        }
        D_800625A0->party->unk2F = 1;
        switch (func_801CA750(0)) {
        case 1:
            D_800625A0->markers->unk144[0] = 0;
            D_800625A0->card->mode = 0;
            dest = D_800625A0->card->cursor / 15 == 0;
            proceed = 1;
            if (!dest) {
                ask = 0x47;
                noCard = 0x4a;
                exists = 0x4d;
            } else {
                ask = 0xbb;
                noCard = 0xbe;
                exists = 0xc1;
            }
            if (!(u8)func_801CACF8(ask, 0xff, 1)) {
                proceed = 0;
                D_800625A0->card->unk4F80 = 0xff;
            }
            if (proceed && !D_800625A0->card->present[dest]) {
                func_801D2F4C(noCard);
                if (!D_800625A0->card->present[dest]) {
                    for (i = 0xb3; i != 0; i--) {
                        VSync(0);
                    }
                    proceed = 0;
                    again = 0;
                }
                func_801D32B4();
            }
            if (proceed && D_801EA900[dest] >= 15) {
                D_801EA900[dest] = 0;
                D_800625A0->party->unk2F = 0;
                func_801CACF8(0xac, 0xff, 0);
                proceed = 0;
                again = 0;
            }
            if (proceed) {
                dest = D_800625A0->card->cursor / 15 == 0;
                if (dest) {
                    __builtin_memcpy(device, "bu00:", 6);
                    __builtin_memcpy(other, "bu10:", 6);
                    src = 0;
                } else {
                    __builtin_memcpy(device, "bu10:", 6);
                    __builtin_memcpy(other, "bu00:", 6);
                    src = 1;
                }
                if (D_800625A0->card->result[dest] == -2) {
                    if (func_801CB8AC(dest)) {
                        func_801D2F4C(0x26);
                        if (format(other)) {
                            func_801D32B4();
                            func_801CACF8(0x5c, 0xff, 0);
                        } else {
                            func_801D32B4();
                            proceed = 0;
                        }
                    } else {
                        D_800625A0->markers->unk144[0] = 1;
                        continue;
                    }
                }
            }
            if (proceed) {
                for (i = 0; i < D_800625A0->card->unk4F8A[dest]; i++) {
                    if (!strcmp(D_800625A0->card->files[D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]]].name,
                                D_800625A0->card->files[dest * 16 + i].name)) {
                        i = 0xff;
                        break;
                    }
                }
                if (i == 0xff) {
                    func_801CACF8(exists, 0xff, 0);
                    proceed = 0;
                }
            }
            if (proceed) {
                func_801D2F4C(0x41);
                D_800625A0->sounds = 0;
                D_800625A0->party->unk2F = 0;
                D_800625A0->party->unkB = 0;
                D_800625A0->card->unk4F80 = 0xff;
                D_800625A0->card->unk4F8C[0] = 0xff;
                D_800625A0->card->unk4F8C[1] = 0xff;
                strcpy(srcName, device);
                strcpy(destName, other);
                strcpy(tempName, other);
                strcat(srcName, D_800625A0->card->files[D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]]].name);
                strcat(destName, D_800625A0->card->files[D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]]].name);
                strcat(tempName, "__tmp_file");
                retry = 3;
                do {
                    fd = open(srcName, 1);
                    if (fd == -1) {
                        fd = 0;
                        func_801C8CA4(src);
                    }
                } while (fd == 0 && --retry != 0);
                if (fd != 0) {
                    srcFd = fd;
                    retry = 3;
                    do {
                        if (read(fd, header, 0x200) == -1) {
                            fd = 0;
                            func_801C8CA4(src);
                        }
                    } while (fd == 0 && --retry != 0);
                    if (fd == 0) {
                        close(srcFd);
                    } else {
                        delete(tempName);
                        retry = 1;
                        do {
                            destFd = open(tempName, header[3] << 16 | 0x200);
                            if (destFd == -1) {
                                fd = 0;
                                full = 1;
                            }
                        } while (fd == 0 && --retry != 0);
                        if (fd == 0) {
                            close(srcFd);
                        }
                        close(destFd);
                        if (fd != 0) {
                            retry = 3;
                            do {
                                destFd = open(tempName, 2);
                                if (destFd == -1) {
                                    fd = 0;
                                    func_801C8CA4(dest);
                                }
                            } while (fd == 0 && --retry != 0);
                            if (fd == 0) {
                                close(srcFd);
                                delete(tempName);
                            } else {
                                retry = 3;
                                do {
                                    if (write(destFd, header, 0x200) == -1) {
                                        fd = 0;
                                        func_801C8CA4(dest);
                                    }
                                } while (fd == 0 && --retry != 0);
                                if (fd == 0) {
                                    close(srcFd);
                                    close(destFd);
                                    destFd = 0;
                                    delete(tempName);
                                }
                                written = 0x200;
                                phase = 0;
                                func_801CAE08(1);
                                for (;;) {
                                    func_801C7BF4();
                                    if (phase == 0) {
                                        retry = 3;
                                        do {
                                            if (read(fd, buffer, 0x200) != 0x200) {
                                                fd = 0;
                                                func_801C8CA4(src);
                                            }
                                        } while (fd == 0 && --retry != 0);
                                        if (fd == 0) {
                                            close(srcFd);
                                            close(destFd);
                                            strcpy(tempName, other);
                                            strcat(tempName, "__tmp_file");
                                            delete(tempName);
                                            break;
                                        }
                                        phase = 1;
                                    } else {
                                        retry = 3;
                                        do {
                                            if (write(destFd, buffer, 0x200) != 0x200) {
                                                fd = 0;
                                                func_801C8CA4(dest);
                                            }
                                        } while (fd == 0 && --retry != 0);
                                        if (fd == 0) {
                                            close(srcFd);
                                            close(destFd);
                                            strcpy(tempName, other);
                                            strcat(tempName, "__tmp_file");
                                            delete(tempName);
                                            break;
                                        }
                                        phase = 0;
                                        written += 0x200;
                                        if (written >= header[3] << 13) {
                                            close(destFd);
                                            close(fd);
                                            strcpy(tempName, other);
                                            strcat(tempName, "__tmp_file");
                                            rename(tempName, destName);
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (proceed) {
                func_801CAE08(2);
                func_801D32B4();
                if (fd != 0) {
                    D_800625A0->sounds = 1;
                    func_801C8574(0x34);
                    D_800625A0->sounds = 0;
                    func_801CACF8(0x5c, 0xff, 0);
                } else {
                    func_801CACF8(!full ? 0x44 : 0xac, 0xff, 0);
                }
                func_801CAE08(0);
                D_800625A0->card->mode = 2;
                while (D_800625A0->cardPollTimer != 1) {
                    func_801C7BF4();
                }
                for (slot = 0; slot < 32; slot++) {
                    D_800625A0->card->fileSlots[slot] = 0xff;
                    D_800625A0->card->ours[slot] = 0;
                    D_800625A0->card->files[slot].state = 0;
                }
                D_800625A0->card->scanned[0] = 0;
                D_800625A0->card->scanned[1] = 0;
                D_801E9778 = 1;
            }
            D_800625A0->markers->unk144[0] = 1;
            D_800625A0->sounds = 1;
            /* fallthrough */
        case 2:
            again = 0;
            break;
        }
    } while (again);
    D_800625A0->card->mode = 1;
    D_800625A0->cardsPresent = 1;
    return result;
}

/* The file screen's delete command: pick a file with the cursor, confirm and
 * erase it, then force the cards to be scanned again. Returns 1 when the
 * card check ended the screen. */
u8 func_801CD2AC(void) {
    char path[64];
    u8 first;
    s32 again;
    u8 ended;
    s32 i;

    first = 1;
    again = 1;
    ended = 0;
    func_801CADB0();
    do {
        if (func_801C93A8()) {
            ended = 1;
            break;
        }
        if (first) {
            first = 0;
            if (D_800625A0->party->messageShown != 0) {
                func_801D32B4();
            }
            D_800625A0->markers->visible[0] = 1;
        }
        if (func_801C9BCC(0) == 0 && (D_800625A0->card->cursor = func_801C9D34(0)) == 0xff) {
            D_800625A0->markers->visible[0] = 0;
            D_800625A0->party->unkB = 0;
            func_801CACF8(0x62, 0xff, 0);
            break;
        }
        D_800625A0->party->unk2F = 1;
        switch (func_801CA750(0)) {
        case 1:
            D_800625A0->markers->unk144[0] = 0;
            D_800625A0->card->mode = 0;
            if ((u8)func_801CACF8(0x56, 0x59, 1)) {
                func_801D2F4C(0x50);
                D_800625A0->party->unk2F = 0;
                D_800625A0->party->unkB = 0;
                D_800625A0->card->unk4F80 = 0xff;
                if (D_800625A0->card->cursor < 15) {
                    __builtin_memcpy(path, "bu00:", 6);
                } else {
                    __builtin_memcpy(path, "bu10:", 6);
                }
                strcat(path, D_800625A0->card->files[D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]]]
                                 .name);
                delete(path);
                func_801D32B4();
                D_800625A0->sounds = 1;
                func_801C8574(0x34);
                D_800625A0->sounds = 0;
                func_801CACF8(0x5c, 0xff, 0);
                D_800625A0->card->mode = 2;
                while (D_800625A0->cardPollTimer != 1) {
                    func_801C7BF4();
                }
                for (i = 0; i < 32; i++) {
                    D_800625A0->card->fileSlots[i] = 0xff;
                    D_800625A0->card->ours[i] = 0;
                    D_800625A0->card->files[i].state = 0;
                }
                D_800625A0->card->scanned[0] = 0;
                D_800625A0->card->scanned[1] = 0;
                D_800625A0->card->unk4F8C[0] = 0xff;
                D_800625A0->card->unk4F8C[1] = 0xff;
                D_801E9778 = 1;
            }
            D_800625A0->markers->unk144[0] = 1;
            /* fallthrough */
        case 2:
            again = 0;
            break;
        }
    } while (again);
    D_800625A0->card->mode = 1;
    D_800625A0->cardsPresent = 1;
    D_800625A0->sounds = 1;
    return ended;
}

/* Run the card command chosen on the file screen (0 copy?, 1 delete?, 2 the
 * save or load of the menu kind); returns 0 when the menu should close. */
u8 func_801CD710(u8 arg) {
    u8 stay;

    func_801D22F4(0);
    D_800625A0->party->unk2F = 0;
    stay = 1;
    switch (D_800625A0->choice) {
    case 0:
        D_800625A0->cardsPresent = 7;
        if (func_801CD2AC()) {
            stay = 0;
        }
        break;
    case 1:
        D_800625A0->cardsPresent = 6;
        if (func_801CC6D8()) {
            stay = 0;
        }
        break;
    case 2:
        if (D_80059460 != 2) {
            D_800625A0->cardsPresent = 3;
            if (func_801CBD90(arg)) {
                stay = 0;
            }
        } else {
            D_800625A0->cardsPresent = 2;
            if (func_801CB304()) {
                stay = 0;
            }
        }
        break;
    }
    func_801D2484();
    return stay;
}

/* Build status panel `panel`'s layout sprites (layout `layout`) for character
 * `ch` on row `row` at the positions of `x` and `y`, its frame sprite (14b +
 * row) and its name label (the character's or, in layout 1, its gear's). */
void func_801CD81C(MenuPanel *panel, u8 ch, u8 row, MenuAnchor *x, MenuAnchor *y, u8 layout) {
    s32 i;

    panel->count0 = 0;
    for (i = 0; i < 9; i++) {
        if (D_801EA4DC[layout * 9 + i] != 0xffff) {
            panel->count0 += func_8002675C(D_800625A0->sheet, D_801EA4DC[layout * 9 + i],
                                           &panel->list0[panel->count0 * 2], D_800625A0->bufferIndex,
                                           x->parts[i], row * 56 + y->parts[i], 0x1000);
        }
    }
    func_8002675C(D_800625A0->sheet, row + 0x14b, panel->frameA, D_800625A0->bufferIndex, x->frame,
                  row * 56 + y->frame, 0x1000);
    func_801E927C(&panel->frameB[D_800625A0->bufferIndex]);
    panel->frameB[D_800625A0->bufferIndex].tpage = GetTPage(0, 0, 0x180, 0);
    if (layout == 0) {
        panel->frameB[D_800625A0->bufferIndex].clut = (ch & 1) ? D_80059414 : D_800595D4;
    } else {
        panel->frameB[D_800625A0->bufferIndex].clut = (D_8006D8A0[ch].gear & 1) ? D_800595D4 : D_80059414;
    }
    func_801E920C(&panel->frameB[D_800625A0->bufferIndex], (u16)x->label, (u16)(y->label + row * 56),
                  (u8)(D_801EA578[layout * 3 + row] * 4), (u8)D_801EA5C4[layout * 3 + row], (layout * 3) * 8 + 0x48,
                  13);
}

/* Lay out character `ch`'s level digits (the last three of +62) at row `row`
 * of `panel` and prepare the +63 digits. */
void func_801CDB1C(MenuPanel *panel, u8 ch, u8 row, MenuAnchor *x, MenuAnchor *y) {
    s32 i;
    u8 digit;

    func_801C80B8(D_8006D8A0[ch].unk62);
    panel->counts[0] = 0;
    for (i = 0; i < 3; i++) {
        digit = D_800625A0->digits[6 + i];
        if (digit != 0xff) {
            panel->counts[0] += func_8002675C(D_800625A0->sheet, digit, &panel->list1[panel->counts[0] * 2],
                                              D_800625A0->bufferIndex, i * 8 + x->base, row * 56 + y->base,
                                              0x1000);
        }
    }
    func_801C80B8(D_8006D8A0[ch].unk63);
    panel->counts[1] = 0;
}

/* Lay out status panel `panel`'s numbers for character `ch` on row `row`:
 * hp (by digit position) and hp maximum (packed) of the character (three
 * digits) or, in layout 1, of its gear (five digits); in layout 0 also ep and
 * ep maximum (two digits). */
void func_801CDC6C(MenuPanel *panel, u8 ch, u8 row, MenuAnchor *x, MenuAnchor *y, u8 layout) {
    s32 digits;
    s32 first;
    s32 i;
    s32 n;
    u8 digit;

    if (layout == 0) {
        func_801C80B8(D_8006D8A0[ch].hp);
        digits = 3;
        first = 6;
    } else {
        digits = 5;
        func_801C80B8(D_8006DFAC[D_8006D8A0[ch].gear].unk60);
        first = 4;
    }
    panel->counts[2] = 0;
    for (i = 0; i < digits; i++) {
        digit = D_800625A0->digits[first + i];
        if (digit != 0xff) {
            panel->counts[2] += func_8002675C(D_800625A0->sheet, digit, &panel->list3[panel->counts[2] * 2],
                                              D_800625A0->bufferIndex, i * 8 + x->hp, row * 56 + y->hp, 0x1000);
        }
    }
    if (layout == 0) {
        func_801C80B8(D_8006D8A0[ch].hpMax);
    } else {
        func_801C80B8(D_8006DFAC[D_8006D8A0[ch].gear].unk64);
    }
    panel->counts[3] = 0;
    for (i = 0, n = 0; i < digits; i++) {
        digit = D_800625A0->digits[first + i];
        if (digit != 0xff) {
            panel->counts[3] += func_8002675C(D_800625A0->sheet, digit, &panel->list4[panel->counts[3] * 2],
                                              D_800625A0->bufferIndex, n * 8 + x->hpMax, row * 56 + y->hpMax, 0x1000);
            n++;
        }
    }
    if (layout == 0) {
        func_801C80B8(D_8006D8A0[ch].ep);
        panel->counts[4] = 0;
        for (i = 0; i < 2; i++) {
            digit = D_800625A0->digits[7 + i];
            if (digit != 0xff) {
                panel->counts[4] += func_8002675C(D_800625A0->sheet, digit, &panel->list5[panel->counts[4] * 2],
                                                  D_800625A0->bufferIndex, i * 8 + x->ep, row * 56 + y->ep, 0x1000);
            }
        }
        func_801C80B8(D_8006D8A0[ch].epMax);
        panel->counts[5] = 0;
        for (i = 0, n = 0; i < 2; i++) {
            digit = D_800625A0->digits[7 + i];
            if (digit != 0xff) {
                panel->counts[5] += func_8002675C(D_800625A0->sheet, digit, &panel->list6[panel->counts[5] * 2],
                                                  D_800625A0->bufferIndex, n * 8 + x->epMax, row * 56 + y->epMax,
                                                  0x1000);
                n++;
            }
        }
    }
}

/* Build the parts of `panel` (801cd81c, 801cdb1c, 801cdc6c) and show it. */
void func_801CE0CC(MenuPanel *panel, u8 a, u8 b, MenuAnchor *c, MenuAnchor *d, u8 e) {
    func_801CD81C(panel, a, b, c, d, e);
    func_801CDB1C(panel, a, b, c, d);
    func_801CDC6C(panel, a, b, c, d, e);
    panel->shown = 1;
    panel->buffer = D_800625A0->bufferIndex;
}

/* Project `count` quads: each takes the next four of `verts` into every
 * other quad of `polys` from `index` and is added to the frame. */
void func_801CE198(s32 count, SVECTOR *verts, POLY_FT4 *polys, s32 first) {
    s32 p;
    s32 flag;
    s32 i;

    for (i = 0; i < count; i++) {
        RotTransPers4(&verts[i * 4], &verts[i * 4 + 1], &verts[i * 4 + 2], &verts[i * 4 + 3],
                      (s32 *)&polys[first + i * 2].x0, (s32 *)&polys[first + i * 2].x1,
                      (s32 *)&polys[first + i * 2].x2, (s32 *)&polys[first + i * 2].x3, &p, &flag);
        AddPrim(&D_800625A0->current->ot[4], &polys[first + i * 2]);
    }
}

/* Add `count` quads of `polys` to the frame, every other one from `first`. */
void func_801CE2B4(s32 count, POLY_FT4 *polys, s32 first) {
    s32 i;

    for (i = 0; i < count; i++) {
        AddPrim(&D_800625A0->current->ot[4], &polys[first + i * 2]);
    }
}

/* Add the shared mode primitive and, while panel 4 shows, its quad. */
void func_801CE338(void) {
    AddPrim(&D_800625A0->current->ot[4], &D_800625A0->primitives->modes0[D_800625A0->primitives->mode]);
    if (D_800625A0->party->redraw4 != 0) {
        AddPrim(&D_800625A0->current->ot[4], &D_800625A0->primitives->polys[D_800625A0->primitives->frame]);
    }
}

/* While party flag +2f is set, add the current quad of each visible marker. */
void func_801CE3C8(void) {
    s32 i;

    if (D_800625A0->party->unk2F != 0) {
        for (i = 0; i < 4; i++) {
            if (D_800625A0->markers->visible[i] != 0) {
                AddPrim(&D_800625A0->current->ot[4],
                              &D_800625A0->markers->polys[i * 2 + D_800625A0->markers->current[i]]);
            }
        }
    }
}

/* Draw the field menu blocks: the command list and its cursor (+340) and the
 * two lists of the second block (+344). */
void func_801CE464(void) {
    if (D_800625A0->party->redraw5 != 0) {
        func_801CE2B4(D_800625A0->fieldMenu->count, D_800625A0->fieldMenu->polys, D_800625A0->fieldMenu->start);
        AddPrim(&D_800625A0->current->ot[4], &D_800625A0->fieldMenu->cursor[D_800625A0->fieldMenu->start]);
    }
    if (D_800625A0->party->redraw6 != 0) {
        func_801CE2B4(7, D_800625A0->fieldMenu2->polys, D_800625A0->fieldMenu2->start);
        func_801CE2B4(4, D_800625A0->fieldMenu2->polys2, D_800625A0->fieldMenu2->start);
    }
}

/* Draw the shown field blocks: their two frame quads and part lists. */
void func_801CE540(void) {
    s32 i;
    MenuFieldBlock *block;

    for (i = 0; i < 3; i++) {
        block = D_800625A0->fieldBlocks[i];
        if (D_800625A0->party->fieldShown[i] != 0) {
            AddPrim(&D_800625A0->current->ot[4], &block->frameA[block->buffer]);
            AddPrim(&D_800625A0->current->ot[4], &block->frameB[block->buffer]);
            func_801CE2B4(block->count0, block->list0, block->buffer);
            func_801CE2B4(block->count1, block->list1, block->buffer);
            func_801CE2B4(block->count2, block->list2, block->buffer);
            func_801CE2B4(block->count3, block->list3, block->buffer);
            func_801CE2B4(block->count4, block->list4, block->buffer);
            func_801CE2B4(block->count5, block->list5, block->buffer);
            func_801CE2B4(block->count6, block->list6, block->buffer);
        }
    }
}

/* While party flag +7 is set, draw the detail panel: frame, portrait, tabs,
 * layout parts and the number lists (level2, value40 and expNext are not
 * drawn here). */
void func_801CE660(void) {
    if (D_800625A0->party->redraw7 != 0) {
        func_801CE198(1, D_800625A0->block358->frameAt, D_800625A0->block358->frame, D_800625A0->block358->buffer);
        func_801CE198(1, D_800625A0->block358->portraitAt, D_800625A0->block358->portrait, D_800625A0->block358->buffer);
        func_801CE198(D_800625A0->block358->tabCount, D_800625A0->block358->tabsAt[0], D_800625A0->block358->tabs, D_800625A0->block358->tabBuffer);
        func_801CE198(D_800625A0->block358->count, D_800625A0->block358->partsAt[0], D_800625A0->block358->parts, D_800625A0->block358->buffer);
        func_801CE198(D_800625A0->block358->hpCount, D_800625A0->block358->hpAt[0], D_800625A0->block358->hp, D_800625A0->block358->buffer);
        func_801CE198(D_800625A0->block358->hpMaxCount, D_800625A0->block358->hpMaxAt[0], D_800625A0->block358->hpMax, D_800625A0->block358->buffer);
        func_801CE198(D_800625A0->block358->epCount, D_800625A0->block358->epAt[0], D_800625A0->block358->ep, D_800625A0->block358->buffer);
        func_801CE198(D_800625A0->block358->epMaxCount, D_800625A0->block358->epMaxAt[0], D_800625A0->block358->epMax, D_800625A0->block358->buffer);
        func_801CE198(D_800625A0->block358->levelCount, D_800625A0->block358->levelAt[0], D_800625A0->block358->level, D_800625A0->block358->buffer);
        func_801CE198(D_800625A0->block358->value3CCount, D_800625A0->block358->value3CAt[0], D_800625A0->block358->value3C, D_800625A0->block358->buffer);
        func_801CE198(D_800625A0->block358->expCount, D_800625A0->block358->expAt[0], D_800625A0->block358->exp, D_800625A0->block358->buffer);
        func_801CE198(D_800625A0->block358->list1C70Count, D_800625A0->block358->list1C70At[0], D_800625A0->block358->list1C70, D_800625A0->block358->buffer);
    }
}

/* Draw the shown rows of the block at +35c: while highlighted, each row's
 * highlight quad and second part list; then its bar and first part list. */
void func_801CE860(void) {
    s32 i;
    s32 p;
    s32 flag;

    for (i = 0; i < 7; i++) {
        if (D_800625A0->block35C->rowShown[i] != 0) {
            if (D_800625A0->block35C->highlighted != 0) {
                RotTransPers4(&D_800625A0->block35C->highlightAt[i][0], &D_800625A0->block35C->highlightAt[i][1], &D_800625A0->block35C->highlightAt[i][2],
                              &D_800625A0->block35C->highlightAt[i][3],
                              (s32 *)&D_800625A0->block35C->highlights[i][D_800625A0->block35C->highlightBuffer[i]].x0,
                              (s32 *)&D_800625A0->block35C->highlights[i][D_800625A0->block35C->highlightBuffer[i]].x1,
                              (s32 *)&D_800625A0->block35C->highlights[i][D_800625A0->block35C->highlightBuffer[i]].x2,
                              (s32 *)&D_800625A0->block35C->highlights[i][D_800625A0->block35C->highlightBuffer[i]].x3, &p, &flag);
                AddPrim(&D_800625A0->current->ot[4], &D_800625A0->block35C->highlights[i][D_800625A0->block35C->highlightBuffer[i]]);
                func_801CE198(D_800625A0->block35C->rowBCount[i], D_800625A0->block35C->rowBAt[i], D_800625A0->block35C->rowB[i], D_800625A0->block35C->rowBBuffer[i]);
            }
            RotTransPers4(&D_800625A0->block35C->barAt[i][0], &D_800625A0->block35C->barAt[i][1], &D_800625A0->block35C->barAt[i][2], &D_800625A0->block35C->barAt[i][3],
                          (s32 *)&D_800625A0->block35C->bars[i][D_800625A0->block35C->barBuffer[i]].x0, (s32 *)&D_800625A0->block35C->bars[i][D_800625A0->block35C->barBuffer[i]].x1,
                          (s32 *)&D_800625A0->block35C->bars[i][D_800625A0->block35C->barBuffer[i]].x2, (s32 *)&D_800625A0->block35C->bars[i][D_800625A0->block35C->barBuffer[i]].x3, &p,
                          &flag);
            AddPrim(&D_800625A0->current->ot[4], &D_800625A0->block35C->bars[i][D_800625A0->block35C->barBuffer[i]]);
            func_801CE198(D_800625A0->block35C->rowACount[i], D_800625A0->block35C->rowAAt[i], D_800625A0->block35C->rowA[i], D_800625A0->block35C->rowABuffer[i]);
        }
    }
}

/* While party flag +8 is set, draw the sprites of the block at +35c. */
void func_801CEB5C(void) {
    MenuBlock35C *block;

    if (D_800625A0->party->unk8 != 0) {
        block = D_800625A0->block35C;
        func_801CE198(block->kind, block->verts, block->polys, block->buffer);
        func_801CE860();
    }
}

/* While party flag +4b is set, draw the visible labels of the block at +360. */
void func_801CEBB4(void) {
    s32 i;

    if (D_800625A0->party->unk4B != 0) {
        for (i = 0; i < 5; i++) {
            if (D_800625A0->labels360->visible[i] != 0) {
                func_801CE198(1, D_800625A0->labels360->labels[i].verts,
                              D_800625A0->labels360->labels[i].polys, D_800625A0->labels360->count);
            }
        }
    }
}

/* While party flag +9 is set, draw both screen image lists, first applying
 * a changed dimming (semi-transparent, 20h grey). */
void func_801CEC40(void) {
    s32 i;

    if (D_800625A0->party->redraw9 != 0) {
        if (D_800625A0->screenImages->captured != D_800625A0->screenImages->refresh) {
            if (D_800625A0->screenImages->captured != 0) {
                for (i = 0; i < D_800625A0->screenImages->count2; i++) {
                    SetSemiTrans(D_800625A0->screenImages->packets2 + (i * 2 + D_800625A0->screenImages->buffer2), 1);
                    SetShadeTex(D_800625A0->screenImages->packets2 + (i * 2 + D_800625A0->screenImages->buffer2), 0);
                    D_800625A0->screenImages->packets2[i * 2 + D_800625A0->screenImages->buffer2].tpage |= 0x20;
                    (D_800625A0->screenImages->packets2 + (i * 2 + D_800625A0->screenImages->buffer2))->r0 = 0x20;
                    (D_800625A0->screenImages->packets2 + (i * 2 + D_800625A0->screenImages->buffer2))->g0 = 0x20;
                    (D_800625A0->screenImages->packets2 + (i * 2 + D_800625A0->screenImages->buffer2))->b0 = 0x20;
                }
                for (i = 0; i < D_800625A0->screenImages->count; i++) {
                    SetSemiTrans(D_800625A0->screenImages->packets + (i * 2 + D_800625A0->screenImages->buffer), 1);
                    SetShadeTex(D_800625A0->screenImages->packets + (i * 2 + D_800625A0->screenImages->buffer), 0);
                    D_800625A0->screenImages->packets[i * 2 + D_800625A0->screenImages->buffer].tpage |= 0x20;
                    (D_800625A0->screenImages->packets + (i * 2 + D_800625A0->screenImages->buffer))->r0 = 0x20;
                    (D_800625A0->screenImages->packets + (i * 2 + D_800625A0->screenImages->buffer))->g0 = 0x20;
                    (D_800625A0->screenImages->packets + (i * 2 + D_800625A0->screenImages->buffer))->b0 = 0x20;
                }
            } else {
                for (i = 0; i < D_800625A0->screenImages->count2; i++) {
                    SetSemiTrans(D_800625A0->screenImages->packets2 + (i * 2 + D_800625A0->screenImages->buffer2), 0);
                    SetShadeTex(D_800625A0->screenImages->packets2 + (i * 2 + D_800625A0->screenImages->buffer2), 0);
                    D_800625A0->screenImages->packets2[i * 2 + D_800625A0->screenImages->buffer2].tpage |= 0x20;
                    (D_800625A0->screenImages->packets2 + (i * 2 + D_800625A0->screenImages->buffer2))->r0 = 0x80;
                    (D_800625A0->screenImages->packets2 + (i * 2 + D_800625A0->screenImages->buffer2))->g0 = 0x80;
                    (D_800625A0->screenImages->packets2 + (i * 2 + D_800625A0->screenImages->buffer2))->b0 = 0x80;
                }
                for (i = 0; i < D_800625A0->screenImages->count; i++) {
                    SetSemiTrans(D_800625A0->screenImages->packets + (i * 2 + D_800625A0->screenImages->buffer), 0);
                    SetShadeTex(D_800625A0->screenImages->packets + (i * 2 + D_800625A0->screenImages->buffer), 0);
                    D_800625A0->screenImages->packets[i * 2 + D_800625A0->screenImages->buffer].tpage |= 0x20;
                    (D_800625A0->screenImages->packets + (i * 2 + D_800625A0->screenImages->buffer))->r0 = 0x80;
                    (D_800625A0->screenImages->packets + (i * 2 + D_800625A0->screenImages->buffer))->g0 = 0x80;
                    (D_800625A0->screenImages->packets + (i * 2 + D_800625A0->screenImages->buffer))->b0 = 0x80;
                }
            }
            D_800625A0->screenImages->refresh = D_800625A0->screenImages->captured;
        }
        func_801CE2B4(D_800625A0->screenImages->count2, D_800625A0->screenImages->packets2, D_800625A0->screenImages->buffer2);
        func_801CE2B4(D_800625A0->screenImages->count, D_800625A0->screenImages->packets, D_800625A0->screenImages->buffer);
    }
}

/* While party flag +a is set, draw the two sprite lists of the block at +354. */
void func_801CF308(void) {
    if (D_800625A0->party->redrawA != 0) {
        func_801CE2B4(D_800625A0->spriteLists->secondCount, D_800625A0->spriteLists->second,
                      D_800625A0->spriteLists->secondStart);
        func_801CE2B4(D_800625A0->spriteLists->firstCount, D_800625A0->spriteLists->first,
                      D_800625A0->spriteLists->firstStart);
    }
}

/* In file selection (+4d8 == 2), draw the pulsing cursor box of every slot
 * of the selected slot's file (only the selected slot when it is empty). */
void func_801CF37C(void) {
    MenuSlotImage *image;
    s32 i;
    s32 p;
    s32 flag;
    u8 file;
    u8 match;

    if (D_800625A0->loadState == 2) {
        file = D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]];
        for (i = 0; i < 32; i++) {
            match = 1;
            if (file == D_800625A0->card->fileSlots[i]) {
                if (file == 0xff) {
                    match = i == D_801E981C[D_800625A0->card->cursor];
                }
                if (match) {
                    image = D_800625A0->images[i];
                    (image->box + D_800625A0->bufferIndex)->r0 = D_800625A0->unk4D4;
                    (image->box + D_800625A0->bufferIndex)->g0 = D_800625A0->unk4D4;
                    (image->box + D_800625A0->bufferIndex)->b0 = D_800625A0->unk4D4;
                    RotTransPers4(&image->iconAt[0], &image->iconAt[1], &image->iconAt[2], &image->iconAt[3],
                                  (s32 *)&image->box[D_800625A0->bufferIndex].x0,
                                  (s32 *)&image->box[D_800625A0->bufferIndex].x1,
                                  (s32 *)&image->box[D_800625A0->bufferIndex].x2,
                                  (s32 *)&image->box[D_800625A0->bufferIndex].x3, &p, &flag);
                    AddPrim(&D_800625A0->current->ot[4], &image->box[D_800625A0->bufferIndex]);
                    AddPrim(&D_800625A0->current->ot[4], &image->boxMode[D_800625A0->bufferIndex]);
                }
            }
        }
    }
}

/* Draw the file icons of card rows `first`..`end` - 1 that are in use into
 * the file screen slots from `firstSlot` on: each icon's texture window (u by row,
 * v by animation frame +4cc) and palette, projected and drawn. */
void func_801CF5E4(s32 first, s32 firstSlot, s32 end) {
    MenuSlotImage *image;
    s32 row;
    s32 slot;
    s32 i;

    row = first;
    slot = firstSlot;
    for (; row < end; row++) {
        if (D_800625A0->card->files[row].state != 0) {
            for (i = 0; i < D_800625A0->card->headers[row][3]; i++, slot++) {
                image = D_800625A0->images[slot];
                (image->icon + D_800625A0->bufferIndex)->u0 = row * 16;
                (image->icon + D_800625A0->bufferIndex)->v0 = D_800625A0->card->files[row].frames[D_800625A0->unk4CC];
                (image->icon + D_800625A0->bufferIndex)->u1 = row * 16 + 16;
                (image->icon + D_800625A0->bufferIndex)->v1 = D_800625A0->card->files[row].frames[D_800625A0->unk4CC];
                (image->icon + D_800625A0->bufferIndex)->u2 = row * 16;
                (image->icon + D_800625A0->bufferIndex)->v2 = D_800625A0->card->files[row].frames[D_800625A0->unk4CC] + 16;
                (image->icon + D_800625A0->bufferIndex)->u3 = row * 16 + 16;
                (image->icon + D_800625A0->bufferIndex)->v3 = D_800625A0->card->files[row].frames[D_800625A0->unk4CC] + 16;
                image->icon[D_800625A0->bufferIndex].clut = GetClut(row * 16, row / 16 + 0x1c1);
                func_801CE198(1, image->iconAt, image->icon, D_800625A0->bufferIndex);
            }
        }
    }
}

/* While the file screen is up, build (while party flag +68 is set) and draw
 * the header of each present card: its label (122 on the cursor's card
 * while +2f is set, else 115) and its name sprite. */
void func_801CF8D8(void) {
    u8 built[2];
    s32 side;
    s32 parts;
    s32 i;
    u8 normal;

    built[1] = 0;
    built[0] = 0;
    if (D_800625A0->loadState != 0) {
        for (side = 0; side < 2; side++) {
            if (D_800625A0->card->present[side] != 0 && D_800625A0->party->cardMode != 0) {
                normal = 1;
                if (D_800625A0->party->unk2F != 0 && side == D_801E981C[D_800625A0->card->cursor] / 16) {
                    normal = 0;
                }
                if (normal) {
                    func_8002675C(D_800625A0->sheet, 0x115, D_800625A0->card->cardHeaders[side].label,
                                  D_800625A0->bufferIndex, side * 0x90 + 0x1e, 0x36, 0x1000);
                } else {
                    func_8002675C(D_800625A0->sheet, 0x122, D_800625A0->card->cardHeaders[side].label,
                                  D_800625A0->bufferIndex, side * 0x90 + 0x1e, 0x36, 0x1000);
                }
                parts = func_8002675C(D_800625A0->sheet, side + 0x162, D_800625A0->card->cardHeaders[side].name,
                                      D_800625A0->bufferIndex, side * 0x90 + 0x1b, 0x36, 0x1000);
                built[side] = 1;
            }
            if (built[side]) {
                for (i = 0; i < parts; i++) {
                    AddPrim(&D_800625A0->current->ot[4],
                            &D_800625A0->card->cardHeaders[side].name[i * 2 + D_800625A0->bufferIndex]);
                }
                AddPrim(&D_800625A0->current->ot[4],
                        &D_800625A0->card->cardHeaders[side].label[D_800625A0->bufferIndex]);
            }
        }
    }
}

/* While the file screen is up, draw the connector lines of every slot but
 * the last of each present card: red within the selected file during file
 * selection, green otherwise. */
void func_801CFB48(void) {
    MenuSlotImage *image;
    s32 i;
    s32 p;
    s32 flag;
    u8 file;
    u8 match;

    if (D_800625A0->loadState != 0) {
        file = D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]];
        for (i = 0; i < 32; i++) {
            image = D_800625A0->images[i];
            if (D_800625A0->card->present[i / 16] && (i / 16) * 16 != i - 15) {
                match = 0;
                if (file == D_800625A0->card->fileSlots[i] && D_800625A0->loadState == 2) {
                    match = 1;
                    if (file == 0xff) {
                        match = i == D_801E981C[D_800625A0->card->cursor];
                    }
                }
                if (match) {
                    (image->lineA + D_800625A0->bufferIndex)->r0 = 0xff;
                    (image->lineA + D_800625A0->bufferIndex)->g0 = 0;
                    (image->lineA + D_800625A0->bufferIndex)->b0 = 0;
                    (image->lineB + D_800625A0->bufferIndex)->r0 = 0xff;
                    (image->lineB + D_800625A0->bufferIndex)->g0 = 0;
                    (image->lineB + D_800625A0->bufferIndex)->b0 = 0;
                } else {
                    (image->lineA + D_800625A0->bufferIndex)->r0 = 0;
                    (image->lineA + D_800625A0->bufferIndex)->g0 = 0xff;
                    (image->lineA + D_800625A0->bufferIndex)->b0 = 0;
                    (image->lineB + D_800625A0->bufferIndex)->r0 = 0;
                    (image->lineB + D_800625A0->bufferIndex)->g0 = 0xff;
                    (image->lineB + D_800625A0->bufferIndex)->b0 = 0;
                }
                RotTransPers3(&image->lineAAt[0], &image->lineAAt[1], &image->lineAAt[3],
                              (s32 *)&image->lineA[D_800625A0->bufferIndex].x0,
                              (s32 *)&image->lineA[D_800625A0->bufferIndex].x1,
                              (s32 *)&image->lineA[D_800625A0->bufferIndex].x2, &p, &flag);
                AddPrim(&D_800625A0->current->ot[4], &image->lineA[D_800625A0->bufferIndex]);
                RotTransPers3(&image->lineBAt[0], &image->lineBAt[2], &image->lineBAt[3],
                              (s32 *)&image->lineB[D_800625A0->bufferIndex].x0,
                              (s32 *)&image->lineB[D_800625A0->bufferIndex].x1,
                              (s32 *)&image->lineB[D_800625A0->bufferIndex].x2, &p, &flag);
                AddPrim(&D_800625A0->current->ot[4], &image->lineB[D_800625A0->bufferIndex]);
            }
        }
    }
}

/* While the card access indicator is shown, draw its bar (as long as the
 * progress +7b0), its sprite and the blinking arrows (steady while closing);
 * while running, advance the progress and wrap it past 100. */
void func_801CFF64(void) {
    if (D_800625A0->party->unk50[2] != 0) {
        setXY4(&MENU_INDICATOR->fills[MENU_INDICATOR->buffer], 0x20, 0x61, MENU_INDICATOR->unk7B0 + 0x20, 0x61,
               0x20, 0x68, MENU_INDICATOR->unk7B0 + 0x20, 0x68);
        if ((u32)D_800625A0->frameCounter % 6 >= 4 || D_800625A0->party->unk50[2] == 2) {
            func_801CE2B4(2, MENU_INDICATOR->spriteB, MENU_INDICATOR->buffer);
        }
        func_801CE2B4(12, MENU_INDICATOR->spriteA, MENU_INDICATOR->buffer);
        AddPrim(&D_800625A0->current->ot[4], &MENU_INDICATOR->fills[MENU_INDICATOR->buffer]);
        if (D_800625A0->party->unk50[2] == 1) {
            if ((MENU_INDICATOR->unk7B0 += MENU_INDICATOR->unk7B4) > 0x100) {
                MENU_INDICATOR->unk7B0 = 0;
            }
        }
    }
}

/* Animate the file screen: its layers, the 15-frame x 6 blink and the
 * 4-step pulse between 4 and 128. */
void func_801D01D0(void) {
    func_801CF37C();
    func_801CF5E4(0, 0, 0x10);
    func_801CF5E4(0x10, 0x10, 0x20);
    func_801CFB48();
    func_801CF8D8();
    func_801CFF64();
    if (++D_800625A0->unk4D0 == 15) {
        D_800625A0->unk4D0 = 0;
        if (++D_800625A0->unk4CC == 6) {
            D_800625A0->unk4CC = 0;
        }
    }
    if (D_800625A0->unk4D9 == 0) {
        D_800625A0->unk4D4 += 4;
        if (D_800625A0->unk4D4 > 0x80) {
            D_800625A0->unk4D9 = 1;
            D_800625A0->unk4D4 = 0x7c;
        }
    } else {
        D_800625A0->unk4D4 -= 4;
        if (D_800625A0->unk4D4 < 0) {
            D_800625A0->unk4D9 = 0;
            D_800625A0->unk4D4 = 4;
        }
    }
}

/* While party flag +b is set, draw the save/load screen's block (+34c):
 * without the file details, the 32 text characters and the selected file's
 * icon cursor (its slot's column and animation step); with them, each shown
 * view's lists, image and name and the shared play time, disc and title
 * lists. Then the shaded band. */
void func_801D02D8(void) {
    s32 i;

    if (!D_800625A0->party->unkB) {
        return;
    }
    if (!D_800625A0->block34C->rebuilt) {
        func_801CE2B4(32, D_800625A0->block34C->chars, D_800625A0->bufferIndex);
        (D_800625A0->block34C->cursor + D_800625A0->bufferIndex)->u0 =
            D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]] * 16;
        (D_800625A0->block34C->cursor + D_800625A0->bufferIndex)->v0 =
            D_800625A0->card->files[D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]]]
                .frames[D_800625A0->unk4CC];
        (D_800625A0->block34C->cursor + D_800625A0->bufferIndex)->u1 =
            D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]] * 16 + 15;
        (D_800625A0->block34C->cursor + D_800625A0->bufferIndex)->v1 =
            D_800625A0->card->files[D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]]]
                .frames[D_800625A0->unk4CC];
        (D_800625A0->block34C->cursor + D_800625A0->bufferIndex)->u2 =
            D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]] * 16;
        (D_800625A0->block34C->cursor + D_800625A0->bufferIndex)->v2 =
            D_800625A0->card->files[D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]]]
                .frames[D_800625A0->unk4CC] + 15;
        (D_800625A0->block34C->cursor + D_800625A0->bufferIndex)->u3 =
            D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]] * 16 + 15;
        (D_800625A0->block34C->cursor + D_800625A0->bufferIndex)->v3 =
            D_800625A0->card->files[D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]]]
                .frames[D_800625A0->unk4CC] + 15;
        D_800625A0->block34C->cursor[D_800625A0->bufferIndex].clut =
            GetClut(D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]] * 16,
                    D_800625A0->card->fileSlots[D_801E981C[D_800625A0->card->cursor]] / 16 + 0x1c1);
        AddPrim(&D_800625A0->current->ot[4], &D_800625A0->block34C->cursor[D_800625A0->bufferIndex]);
    } else {
        for (i = 0; i < 3; i++) {
            if (D_800625A0->block34C->views[i].shown) {
                func_801CE2B4(D_800625A0->block34C->views[i].frameCount, D_800625A0->block34C->views[i].frame[0],
                              D_800625A0->block34C->views[i].frameBuffer);
                func_801CE2B4(D_800625A0->block34C->views[i].levelCount, D_800625A0->block34C->views[i].levelDigits[0],
                              D_800625A0->block34C->views[i].buffer);
                func_801CE2B4(D_800625A0->block34C->views[i].unk871, D_800625A0->block34C->views[i].levelDigits[3],
                              D_800625A0->block34C->views[i].buffer);
                func_801CE2B4(D_800625A0->block34C->views[i].aCount, D_800625A0->block34C->views[i].aDigits[0],
                              D_800625A0->block34C->views[i].buffer);
                func_801CE2B4(D_800625A0->block34C->views[i].bCount, D_800625A0->block34C->views[i].bDigits[0],
                              D_800625A0->block34C->views[i].buffer);
                func_801CE2B4(D_800625A0->block34C->views[i].cCount, D_800625A0->block34C->views[i].cDigits[0],
                              D_800625A0->block34C->views[i].buffer);
                func_801CE2B4(D_800625A0->block34C->views[i].dCount, D_800625A0->block34C->views[i].dDigits[0],
                              D_800625A0->block34C->views[i].buffer);
                AddPrim(&D_800625A0->current->ot[4],
                        &D_800625A0->block34C->views[i].image[D_800625A0->block34C->views[0].buffer]);
                AddPrim(&D_800625A0->current->ot[4],
                        &D_800625A0->block34C->views[i].name[D_800625A0->block34C->views[i].nameBuffer]);
            }
        }
        func_801CE2B4(11, D_800625A0->block34C->colon0, D_800625A0->block34C->views[0].buffer);
        func_801CE2B4(4, D_800625A0->block34C->discLabel, D_800625A0->block34C->views[0].buffer);
        func_801CE2B4(16, D_800625A0->block34C->title, D_800625A0->block34C->views[0].buffer);
    }
    AddPrim(&D_800625A0->current->ot[4], &D_800625A0->block34C->band[D_800625A0->bufferIndex]);
}

/* Project the four vertices `v` into quad `index` of `polys` and add it at
 * depth `otz`. */
void func_801D0954(SVECTOR *v, POLY_FT4 *polys, s32 index, s32 otz) {
    s32 p;
    s32 flag;
    POLY_FT4 *poly;

    poly = &polys[index];
    RotTransPers4(&v[0], &v[1], &v[2], &v[3], (s32 *)&poly->x0, (s32 *)&poly->x1, (s32 *)&poly->x2,
                  (s32 *)&poly->x3, &p, &flag);
    AddPrim(&D_800625A0->current->ot[otz], poly);
}

/* Draw portrait window `index`: its corners, the frame sprites when
 * `framed`, the edges and the fill. */
void func_801D09F0(s32 index, u8 framed) {
    MenuPortrait *portrait;
    s32 i;
    s32 p;
    s32 flag;

    portrait = D_800625A0->portraits[index];
    for (i = 0; i < 4; i++) {
        func_801D0954(&portrait->cornerAt[i * 4], &portrait->corner[i * 2], portrait->buffer, portrait->depth);
    }
    if (framed) {
        for (i = 0; i < 2; i++) {
            func_801D0954(&portrait->endsAt[i * 4], &portrait->frameEnds[i * 2], portrait->buffer, portrait->depth);
        }
        func_801D0954(portrait->sideAt, portrait->frameSide, portrait->buffer, portrait->depth);
    }
    func_801D0954(portrait->edgeAt[0][0], &portrait->edge[0][0], portrait->buffer, portrait->depth);
    func_801D0954(portrait->edgeAt[0][1], &portrait->edge[0][2], portrait->buffer, portrait->depth);
    func_801D0954(portrait->edgeAt[1][0], &portrait->edge[1][0], portrait->buffer, portrait->depth);
    func_801D0954(portrait->edgeAt[1][1], &portrait->edge[1][2], portrait->buffer, portrait->depth);
    func_801D0954(portrait->edgeAt[2][0], &portrait->edge[2][0], portrait->buffer, portrait->depth);
    func_801D0954(portrait->edgeAt[2][1], &portrait->edge[2][2], portrait->buffer, portrait->depth);
    func_801D0954(portrait->edgeAt[3][0], &portrait->edge[3][0], portrait->buffer, portrait->depth);
    func_801D0954(portrait->edgeAt[3][1], &portrait->edge[3][2], portrait->buffer, portrait->depth);
    RotTransPers4(&portrait->fillAt[0], &portrait->fillAt[1], &portrait->fillAt[2], &portrait->fillAt[3],
                  (s32 *)&portrait->fill[portrait->buffer].x0, (s32 *)&portrait->fill[portrait->buffer].x1,
                  (s32 *)&portrait->fill[portrait->buffer].x2, (s32 *)&portrait->fill[portrait->buffer].x3, &p,
                  &flag);
    AddPrim(&D_800625A0->current->ot[portrait->depth], &portrait->fill[portrait->buffer]);
    AddPrim(&D_800625A0->current->ot[portrait->depth], &portrait->fillMode[portrait->buffer]);
}

/* Draw the shown portrait windows; those of style 0 are drawn under an
 * identity rotation at depth 0x200. */
void func_801D0C78(void) {
    s32 i;
    SVECTOR angles;
    VECTOR offset;
    MATRIX m;
    SVECTOR unused;

    for (i = 0; i < 7; i++) {
        if (D_800625A0->party->unk20[i] != 0) {
            if (D_800625A0->portraits[i]->style == 0) {
                PushMatrix();
                angles.vz = 0;
                angles.vy = 0;
                angles.vx = 0;
                offset.vy = 0;
                offset.vx = 0;
                offset.vz = 0x200;
                func_8003F738(&angles, &m);
                TransMatrix(&m, &offset);
                SetRotMatrix(&m);
                SetTransMatrix(&m);
                func_801D09F0(i, D_800625A0->portraits[i]->framed);
                PopMatrix();
            } else {
                func_801D09F0(i, D_800625A0->portraits[i]->framed);
            }
        }
    }
}

/* Add the current quad of each top label whose party flag (+34) is set. */
void func_801D0D90(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (D_800625A0->party->unk34[i] != 0) {
            AddPrim(&D_800625A0->current->ot[4],
                          &D_800625A0->topLabels[i].polys[D_800625A0->topLabels[i].count]);
        }
    }
}

/* A short busy delay (eight iterations). */
void func_801D0E20(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
    }
}

/* Draw the sprites of labels 8-13 whose party flags (+14) and own flags are set. */
void func_801D0E38(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (D_800625A0->party->unk14[i] != 0 && D_800625A0->partyLabels[i].visible != 0) {
            func_801CE198(1, D_800625A0->partyLabels[i].verts, D_800625A0->partyLabels[i].polys,
                          D_800625A0->partyLabels[i].count);
        }
    }
}

/* A short busy delay (six iterations). */
void func_801D0EBC(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
    }
}

/* Draw the sprites of labels 20-27 whose party flags (+38) are set. */
void func_801D0ED4(void) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (D_800625A0->party->unk38[i] != 0) {
            func_801CE198(1, D_800625A0->labels10E0[i].verts, D_800625A0->labels10E0[i].polys,
                          D_800625A0->labels10E0[i].count);
        }
    }
}

/* Draw the sprites of labels 28-33 whose party flags (+40) are set. */
void func_801D0F54(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (D_800625A0->party->unk40[i] != 0) {
            func_801CE198(1, D_800625A0->labels14E0[i].verts, D_800625A0->labels14E0[i].polys,
                          D_800625A0->labels14E0[i].count);
        }
    }
}

/* While party flag +4e is set, add label 17e0's current quad to the frame. */
void func_801D0FD4(void) {
    if (D_800625A0->party->unk4E != 0) {
        AddPrim(&D_800625A0->current->ot[4],
                      &D_800625A0->labels17E0[0].polys[D_800625A0->labels17E0[0].count]);
    }
}

/* While party flag +2e is set, draw the three notice labels (+1de0): their
 * sprites when visible, else their current quad. */
void func_801D1030(void) {
    s32 i;
    MenuLabelSlot *label;

    if (D_800625A0->party->unk2E != 0) {
        for (i = 0; i < 3; i++) {
            label = D_800625A0->blocks1DE0[i];
            if (label->visible != 0) {
                func_801CE198(1, label->verts, label->polys, label->count);
            } else {
                AddPrim(&D_800625A0->current->ot[4], &label->polys[label->count]);
            }
        }
    }
}

/* Draw the visible labels at +18e0 whose party flags (+54) are set. */
void func_801D10DC(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (D_800625A0->party->unk54[i] != 0 && D_800625A0->labels18E0[i].visible != 0) {
            func_801CE198(1, D_800625A0->labels18E0[i].verts, D_800625A0->labels18E0[i].polys,
                          D_800625A0->labels18E0[i].count);
        }
    }
}

/* Add the current quad of each sound label whose party flag (+5c) is set. */
void func_801D1160(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        if (D_800625A0->party->unk5C[i] != 0) {
            AddPrim(&D_800625A0->current->ot[4],
                          &D_800625A0->soundLabels[i].polys[D_800625A0->soundLabels[i].count]);
        }
    }
}

/* Draw the label layers of the field menu screen. */
void func_801D11F0(void) {
    func_801D0D90();
    func_801D0E20();
    func_801D0E38();
    func_801D0EBC();
    func_801D10DC();
    func_801D1160();
    func_801D0ED4();
    func_801D0F54();
    func_801D0FD4();
    func_801D1030();
}

/* Add this buffer's fill and mode primitives to the frame. */
void func_801D1258(void) {
    AddPrim(&D_800625A0->current->ot[8], &D_800625A0->primitives->fills[D_800625A0->bufferIndex]);
    AddPrim(&D_800625A0->current->ot[8], &D_800625A0->primitives->modes[D_800625A0->bufferIndex]);
}

/* Draw `panel` when shown: its frame quads and part lists (and the extra list
 * when `extra`). */
void func_801D12D4(MenuPanel *panel, u8 extra) {
    if (panel->shown != 0) {
        AddPrim(&D_800625A0->current->ot[4], &panel->frameA[panel->buffer]);
        AddPrim(&D_800625A0->current->ot[4], &panel->frameB[panel->buffer]);
        func_801CE2B4(panel->count0, panel->list0, panel->buffer);
        func_801CE2B4(panel->counts[0], panel->list1, panel->buffer);
        func_801CE2B4(panel->counts[1], panel->list2, panel->buffer);
        func_801CE2B4(panel->counts[2], panel->list3, panel->buffer);
        func_801CE2B4(panel->counts[3], panel->list4, panel->buffer);
        func_801CE2B4(panel->counts[4], panel->list5, panel->buffer);
        func_801CE2B4(panel->counts[5], panel->list6, panel->buffer);
        if (extra) {
            func_801CE2B4(5, panel->extra, panel->buffer);
        }
    }
}

/* While party flag +46 is set, draw the three portrait panels (+1e08). */
void func_801D13F8(void) {
    s32 i;

    if (D_800625A0->party->unk46 != 0) {
        for (i = 0; i < 3; i++) {
            func_801D12D4(D_800625A0->panels[i], 1);
        }
    }
}

/* While party flag +49 is set, draw the sprites of the block at +43c. */
void func_801D1464(void) {
    MenuBlock43C *block;

    if (D_800625A0->party->unk49 != 0) {
        block = D_800625A0->block43C;
        func_801CE198(1, block->verts, block->polys, block->buffer);
    }
}

/* While party flag +53 is set, draw the sprites of the block at +440. */
void func_801D14B0(void) {
    MenuBlock440 *block;

    if (D_800625A0->party->unk53 != 0) {
        block = D_800625A0->block440;
        func_801CE198(4, block->verts, block->polys, block->buffer);
    }
}

/* While party flag +48 is set, draw the save/load screen list (+42c): each
 * shown row's name and value and, when shown, the three extra labels. */
void func_801D14FC(void) {
    s32 i;

    if (D_800625A0->party->unk48 != 0) {
        for (i = 0; i < 16; i++) {
            if (D_800625A0->block42C->shown[i] != 0) {
                func_801CE198(1, D_800625A0->block42C->names[i].verts, D_800625A0->block42C->names[i].polys,
                              D_800625A0->block42C->names[i].count);
                func_801CE198(1, D_800625A0->block42C->values[i].verts, D_800625A0->block42C->values[i].polys,
                              D_800625A0->block42C->values[i].count);
            }
        }
        if (D_800625A0->block42C->extraShown != 0) {
            func_801CE198(1, D_800625A0->block42C->extra[0].verts, D_800625A0->block42C->extra[0].polys,
                          D_800625A0->block42C->extra[0].count);
            func_801CE198(1, D_800625A0->block42C->extra[1].verts, D_800625A0->block42C->extra[1].polys,
                          D_800625A0->block42C->extra[1].count);
            func_801CE198(1, D_800625A0->block42C->extra[2].verts, D_800625A0->block42C->extra[2].polys,
                          D_800625A0->block42C->extra[2].count);
        }
    }
}

/* While party flag +4a is set, draw the file list (+430): each shown row's
 * name and value, when shown the header labels, and the footer. */
void func_801D1640(void) {
    s32 i;

    if (D_800625A0->party->unk4A != 0) {
        for (i = 0; i < 14; i++) {
            if (D_800625A0->block430->shown[i] != 0) {
                func_801CE198(1, D_800625A0->block430->names[i].verts, D_800625A0->block430->names[i].polys,
                              D_800625A0->block430->names[i].count);
                func_801CE198(1, D_800625A0->block430->values[i].verts, D_800625A0->block430->values[i].polys,
                              D_800625A0->block430->values[i].count);
            }
        }
        if (D_800625A0->block430->extraShown != 0) {
            func_801CE198(1, D_800625A0->block430->headA.verts, D_800625A0->block430->headA.polys,
                          D_800625A0->block430->headA.count);
            func_801CE198(1, D_800625A0->block430->headB.verts, D_800625A0->block430->headB.polys,
                          D_800625A0->block430->headB.count);
            for (i = 0; i < 2; i++) {
                func_801CE198(1, D_800625A0->block430->extra[i].verts, D_800625A0->block430->extra[i].polys,
                              D_800625A0->block430->extra[i].count);
            }
        }
        func_801CE198(1, D_800625A0->block430->footer.verts, D_800625A0->block430->footer.polys,
                      D_800625A0->block430->footer.count);
    }
}

/* While party flag +4c is set, draw the list of the block at +434: each shown
 * row's name and value, the title and, when shown, the three extra labels. */
void func_801D17C4(void) {
    s32 i;

    if (D_800625A0->party->unk4C != 0) {
        for (i = 0; i < 8; i++) {
            if (D_800625A0->block434->shown[i] != 0) {
                func_801CE198(1, D_800625A0->block434->names[i].verts, D_800625A0->block434->names[i].polys,
                              D_800625A0->block434->names[i].count);
                func_801CE198(1, D_800625A0->block434->values[i].verts, D_800625A0->block434->values[i].polys,
                              D_800625A0->block434->values[i].count);
            }
        }
        func_801CE198(1, D_800625A0->block434->title.verts, D_800625A0->block434->title.polys,
                      D_800625A0->block434->title.count);
        if (D_800625A0->block434->extraShown != 0) {
            for (i = 0; i < 3; i++) {
                func_801CE198(1, D_800625A0->block434->extra[i].verts, D_800625A0->block434->extra[i].polys,
                              D_800625A0->block434->extra[i].count);
            }
        }
    }
}

/* While party flag +4d is set, draw the status list (+438): each shown row's
 * name and value quads, the title, each row's parts and its gauge. */
void func_801D1914(void) {
    s32 i;

    if (D_800625A0->party->unk4D != 0) {
        for (i = 0; i < 13; i++) {
            if (D_800625A0->block438->shown[i] != 0) {
                AddPrim(&D_800625A0->current->ot[4],
                              &D_800625A0->block438->names[i].polys[D_800625A0->block438->names[i].count]);
                AddPrim(&D_800625A0->current->ot[4],
                              &D_800625A0->block438->values[i].polys[D_800625A0->block438->names[i].count]);
            }
        }
        func_801CE198(1, D_800625A0->block438->title.verts, D_800625A0->block438->title.polys,
                      D_800625A0->block438->title.count);
        for (i = 0; i < 13; i++) {
            func_801CE2B4(D_800625A0->block438->counts[i], D_800625A0->block438->lists[i],
                          D_800625A0->block438->starts[i]);
            if (D_800625A0->block438->gaugeShown[i] != 0) {
                AddPrim(&D_800625A0->current->ot[4],
                              &D_800625A0->block438->gauges[i][D_800625A0->block438->gaugeBuffer[i]]);
            }
        }
    }
}

/* Draw the sprites of the two image blocks (+444) whose party flags are set. */
void func_801D1AAC(void) {
    s32 i;
    MenuImageBlock *block;

    for (i = 0; i < 2; i++) {
        if (D_800625A0->party->unk50[i] != 0) {
            block = D_800625A0->blocks444[i];
            func_801CE198(1, block->verts, block->polys, block->count);
        }
    }
}

/* Build the field menu screen, layer by layer. */
void func_801D1B20(void) {
    func_801D3B00();
    func_801D11F0();
    func_801CE3C8();
    func_801CE338();
    func_801D02D8();
    func_801D01D0();
    func_801CE540();
    func_801CE660();
    func_801CEB5C();
    func_801CEBB4();
    func_801CE464();
    func_801D13F8();
    func_801D1AAC();
    func_801D14FC();
    func_801D1640();
    func_801D17C4();
    func_801D1914();
    func_801D1464();
    func_801D14B0();
    func_801D0C78();
    func_801CEC40();
    func_801CF308();
}

/* Draw one layer set of the field menu screen. */
void func_801D1BE8(void) {
    func_801D3B00();
    func_801D11F0();
    func_801CE3C8();
    func_801CE338();
    func_801D02D8();
    func_801D01D0();
    func_801CEC40();
    func_801CF308();
    func_801D0C78();
}

/* Draw the other layer set of the field menu screen. */
void func_801D1C48(void) {
    func_801D3B00();
    func_801CE3C8();
    func_801D11F0();
    func_801CE338();
    func_801D02D8();
    func_801D01D0();
    func_801D0C78();
    func_801CF308();
}

/* Build the screen of the menu kind (when drawing), then the shared prims. */
void func_801D1CA0(void) {
    if (D_800625A0->drawing != 0) {
        switch (D_80059460) {
        case 0:
            func_801D1B20();
            break;
        case 2:
            func_801D1BE8();
            break;
        case 6:
            func_801D1C48();
            break;
        }
    }
    func_801D1258();
}

/* Step the view motion (3/4 start moving in/out, 1/2 move) and load the view
 * rotation and translation into the GTE. */
void func_801D1D40(void) {
    MenuState *state;
    u8 motion;

    state = D_800625A0;
    switch (state->viewMotion) {
    case 4:
        state->viewOffset.vz = 0x200;
        motion = 2;
        goto start;
    case 3:
        state->viewOffset.vz = 0x800;
        motion = 1;
    start:
        state->viewAngles.vz = 0;
        state->viewAngles.vy = 0;
        state->viewAngles.vx = 0;
        state->viewOffset.vy = 0;
        state->viewOffset.vx = 0;
        state->viewMotion = motion;
        break;
    case 2:
        state->viewAngles.vy -= 0x60;
        state->viewOffset.vz += 0x40;
        if (state->viewOffset.vz >= 0xe00) {
            state->viewMotion = 0;
        }
        break;
    case 1:
        state->viewAngles.vx += 0x7c;
        state->viewOffset.vz -= 0x30;
        if (state->viewOffset.vz < 0x200) {
            state->viewOffset.vz = 0x200;
            state->viewAngles.vz = 0;
            state->viewAngles.vx = 0;
            state->viewAngles.vy = 0;
            state->viewMotion = 0;
        }
        break;
    }
    func_8003F738(&D_800625A0->viewAngles, &D_800625A0->viewMatrix);
    TransMatrix(&D_800625A0->viewMatrix, &D_800625A0->viewOffset);
    SetRotMatrix(&D_800625A0->viewMatrix);
    SetTransMatrix(&D_800625A0->viewMatrix);
}

/* Start the view moving in (3) with its sound. */
void func_801D1E80(void) {
    D_800625A0->viewMotion = 3;
    func_801C8574(0x5b);
}

/* Start the view moving out (4) with its sound. */
void func_801D1EB0(void) {
    D_800625A0->viewMotion = 4;
    func_801C8574(0x5c);
}

/* Place the highlight sprite at position `index`; with `outline` also its
 * background box and outline around the text, as wide as the block's +15b. */
void func_801D1EE0(s32 index, u8 outline) {
    func_8002675C(D_800625A0->sheet, 0x108, D_800625A0->primitives, D_800625A0->bufferIndex, D_801E9A00[index], D_801E9A2C[index], 0x1000);
    D_800625A0->primitives->frame = D_800625A0->bufferIndex;
    if (outline) {
        (D_800625A0->primitives->box + D_800625A0->bufferIndex)->x0 = D_801E9A00[index] + 0x14;
        (D_800625A0->primitives->box + D_800625A0->bufferIndex)->y0 = D_801E9A2C[index] - 0x24;
        (D_800625A0->primitives->box + D_800625A0->bufferIndex)->x1 = D_801E9A00[index] + (D_800625A0->primitives->shade + 0x14);
        (D_800625A0->primitives->box + D_800625A0->bufferIndex)->y1 = D_801E9A2C[index] - 0x24;
        (D_800625A0->primitives->box + D_800625A0->bufferIndex)->x2 = D_801E9A00[index] + 0x14;
        (D_800625A0->primitives->box + D_800625A0->bufferIndex)->y2 = D_801E9A2C[index] - 0x14;
        (D_800625A0->primitives->box + D_800625A0->bufferIndex)->x3 = D_801E9A00[index] + (D_800625A0->primitives->shade + 0x14);
        (D_800625A0->primitives->box + D_800625A0->bufferIndex)->y3 = D_801E9A2C[index] - 0x14;
        (D_800625A0->primitives->edgeA + D_800625A0->bufferIndex)->x0 = D_801E9A00[index] + 0x14;
        (D_800625A0->primitives->edgeA + D_800625A0->bufferIndex)->y0 = D_801E9A2C[index] - 0x24;
        (D_800625A0->primitives->edgeA + D_800625A0->bufferIndex)->x1 = D_801E9A00[index] + (D_800625A0->primitives->shade + 0x14);
        (D_800625A0->primitives->edgeA + D_800625A0->bufferIndex)->y1 = D_801E9A2C[index] - 0x24;
        (D_800625A0->primitives->edgeA + D_800625A0->bufferIndex)->x2 = D_801E9A00[index] + (D_800625A0->primitives->shade + 0x14);
        (D_800625A0->primitives->edgeA + D_800625A0->bufferIndex)->y2 = D_801E9A2C[index] - 0x14;
        (D_800625A0->primitives->edgeB + D_800625A0->bufferIndex)->x0 = D_801E9A00[index] + 0x14;
        (D_800625A0->primitives->edgeB + D_800625A0->bufferIndex)->y0 = D_801E9A2C[index] - 0x24;
        (D_800625A0->primitives->edgeB + D_800625A0->bufferIndex)->x1 = D_801E9A00[index] + 0x14;
        (D_800625A0->primitives->edgeB + D_800625A0->bufferIndex)->y1 = D_801E9A2C[index] - 0x14;
        (D_800625A0->primitives->edgeB + D_800625A0->bufferIndex)->x2 = D_801E9A00[index] + (D_800625A0->primitives->shade + 0x14);
        (D_800625A0->primitives->edgeB + D_800625A0->bufferIndex)->y2 = D_801E9A2C[index] - 0x14;
        D_800625A0->primitives->mode = D_800625A0->bufferIndex;
        D_800625A0->party->redraw3 = 1;
    }
}

/* Hide the party panels (redraw flags 3 and 4). */
void func_801D22C4(void) {
    D_800625A0->party->redraw4 = 0;
    D_800625A0->party->redraw3 = 0;
}

/* Lay out the screen markers for `mode`: 0 all four and the two extra flags,
 * 2 all four, 3 the first at the origin, 1 none. */
void func_801D22F4(u8 mode) {
    s32 i;

    D_800625A0->party->unk2F = 0;
    switch (mode) {
    case 0:
        D_800625A0->party->unk2F = 1;
        D_800625A0->markers->unk144[0] = 1;
        D_800625A0->markers->unk144[1] = 1;
    case 2:
        for (i = 0; i < 4; i++) {
            func_8002675C(D_800625A0->sheet, 0x108, &D_800625A0->markers->polys[i * 2], D_800625A0->bufferIndex,
                          D_801E9A58[i], D_801E9A68[i], 0x800);
            D_800625A0->markers->current[i] = D_800625A0->bufferIndex;
        }
        break;
    case 3:
        func_8002675C(D_800625A0->sheet, 0x108, D_800625A0->markers->polys, D_800625A0->bufferIndex, 0, 0, 0x800);
        D_800625A0->markers->current[0] = D_800625A0->bufferIndex;
    case 1:
        break;
    }
}

/* Clear the party block flag at +2f. */
void func_801D2484(void) {
    D_800625A0->party->unk2F = 0;
}

/* Show (`show`) the six party labels, placing labels 3-5 as quads, or hide
 * the party name labels. */
void func_801D249C(u8 show) {
    s32 i;

    if (show) {
        func_801E7E68(D_800625A0->partyLabels, D_801EA534, 4, 6);
        for (i = 0; i < 3; i++) {
            func_801C851C(D_800625A0->partyLabels[3 + i].verts, D_801E9E4C[i], D_801E9E58[i],
                          D_800625A0->partyLabels[3 + i].width, 0xd);
            D_800625A0->partyLabels[3 + i].count = D_800625A0->bufferIndex;
            D_800625A0->partyLabels[3 + i].visible = 1;
            D_800625A0->party->unk14[3 + i] = 1;
        }
    } else {
        for (i = 0; i < 3; i++) {
            D_800625A0->party->unk14[i] = 0;
        }
    }
}

/* Clear the party block's six bytes at +14. */
void func_801D25E4(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        D_800625A0->party->unk14[i] = 0;
    }
}

/* Hide the party names and place the name label of the current choice as a
 * quad beside its highlight. */
void func_801D261C(void) {
    func_801D249C(0);
    ((D_800625A0->partyLabels + D_800625A0->choice)->polys + D_800625A0->bufferIndex)->x0 =
        D_801E9A1C[D_800625A0->choice] + 0x16 + D_801E9E94[D_800625A0->choice];
    ((D_800625A0->partyLabels + D_800625A0->choice)->polys + D_800625A0->bufferIndex)->y0 =
        D_801E9A48[D_800625A0->choice] - 0x22;
    ((D_800625A0->partyLabels + D_800625A0->choice)->polys + D_800625A0->bufferIndex)->x1 =
        D_801E9A1C[D_800625A0->choice] + 0x16 + D_801E9E94[D_800625A0->choice] +
        D_800625A0->partyLabels[D_800625A0->choice].width;
    ((D_800625A0->partyLabels + D_800625A0->choice)->polys + D_800625A0->bufferIndex)->y1 =
        D_801E9A48[D_800625A0->choice] - 0x22;
    ((D_800625A0->partyLabels + D_800625A0->choice)->polys + D_800625A0->bufferIndex)->x2 =
        D_801E9A1C[D_800625A0->choice] + 0x16 + D_801E9E94[D_800625A0->choice];
    ((D_800625A0->partyLabels + D_800625A0->choice)->polys + D_800625A0->bufferIndex)->y2 =
        D_801E9A48[D_800625A0->choice] - 0x15;
    ((D_800625A0->partyLabels + D_800625A0->choice)->polys + D_800625A0->bufferIndex)->x3 =
        D_801E9A1C[D_800625A0->choice] + 0x16 + D_801E9E94[D_800625A0->choice] +
        D_800625A0->partyLabels[D_800625A0->choice].width;
    ((D_800625A0->partyLabels + D_800625A0->choice)->polys + D_800625A0->bufferIndex)->y3 =
        D_801E9A48[D_800625A0->choice] - 0x15;
    D_800625A0->partyLabels[D_800625A0->choice].count = D_800625A0->bufferIndex;
    D_800625A0->party->unk14[D_800625A0->choice] = 1;
}

/* Draw the small window at (d4, b2) and its element at (d8, b6). */
void func_801D28A8(void) {
    func_801D397C(0, 0xd4, 0xb2, 0x60, 0x10, 0, 0, 4, 0);
    func_801D5BA4(0xd8, 0xb6);
}

/* Open the small window at (cc, c6) with its element and show it. */
void func_801D28FC(void) {
    func_801D397C(1, 0xcc, 0xc6, 0x50, 0x10, 0, 0, 4, 0);
    func_801D5CF8(0xd0, 0xca);
    D_800625A0->party->redraw6 = 1;
}

/* Draw the element at (d0, ca) while party flag 6 is set. */
void func_801D2968(void) {
    if (D_800625A0->party->redraw6 != 0) {
        func_801D5CF8(0xd0, 0xca);
    }
}

/* Slide the three party panels in (`open`) or out along their movers,
 * drawing each frame until one arrives; opening then pins them in place,
 * plays the open sound and (unless `keep`) redraws the status. */
void func_801D29A8(u8 open, u8 keep) {
    MenuParty *party;
    s32 i;

    if (open) {
        func_801E8018(8, D_800625A0->commandLabels, D_801EA528, D_800625A0->party->labels);
        func_801C81E0(0x100, 0x86, 0x60, 6, 8, 0);
        func_801C81E0(0x108, 0x3e, 0x68, 0x3e, 8, 1);
        func_801C81E0(0x110, -10, 0x70, 0x76, 8, 2);
    } else {
        func_801E8044(8, D_800625A0->party->labels);
        func_801C81E0(0x60, 6, 0x100, 0x86, 8, 0);
        func_801C81E0(0x68, 0x3e, 0x108, 0x3e, 8, 1);
        func_801C81E0(0x70, 0x76, 0x110, -10, 8, 2);
    }
    while (D_800625A0->movers[0].done == 0 && D_800625A0->movers[1].done == 0 &&
           D_800625A0->movers[2].done == 0) {
        for (i = 0; i < 3; i++) {
            if (D_800625A0->party->ids[i] != 0xff) {
                func_801D5A50(i, D_800625A0->party->ids[i]);
            }
        }
        func_801C7BF4();
        for (i = 0; i < 3; i++) {
            if (D_800625A0->party->ids[i] != 0xff) {
                func_801C8324(i);
            }
        }
    }
    if (open) {
        D_800625A0->movers[0].x0 = 0x60;
        D_800625A0->movers[0].y0 = 6;
        D_800625A0->movers[1].x0 = 0x68;
        D_800625A0->movers[1].y0 = 0x3e;
        D_800625A0->movers[2].x0 = 0x70;
        D_800625A0->movers[2].y0 = 0x76;
        D_800625A0->movers[0].accY = 0;
        D_800625A0->movers[0].accX = 0;
        D_800625A0->movers[1].accY = 0;
        D_800625A0->movers[1].accX = 0;
        D_800625A0->movers[2].accY = 0;
        D_800625A0->movers[2].accX = 0;
        for (i = 0; i < 3; i++) {
            if (D_800625A0->party->ids[i] != 0xff) {
                func_801D5A50(i, D_800625A0->party->ids[i]);
            }
        }
        func_801C8574(0x5d);
        if (!keep) {
            func_801D28A8();
        }
        D_800625A0->party->unk20[1] = 1;
        D_800625A0->party->redraw6 = 1;
    } else {
        party = D_800625A0->party;
        party->fieldShown[2] = 0;
        party->fieldShown[1] = 0;
        party->fieldShown[0] = 0;
        if (!keep) {
            D_800625A0->party->unk20[0] = 0;
            D_800625A0->party->redraw5 = 0;
        }
    }
    func_801C7BF4();
}

/* Open the field menu: the two top portraits, then each party member's and
 * gear's name image, the command cursor, panels and money window. */
void func_801D2D38(void) {
    s32 i;
    s32 row;
    void *block;
    s32 gear;

    if (D_80059460 == 0) {
        for (row = 0; row < 2; row++) {
            block = func_80031BDC(0x720, 0);
            D_800625A0->portraits[row] = block;
            bzero(block, 0x720);
            block = func_80031BDC(0x18, 0);
            D_800625A0->portraitMarks[row] = block;
            bzero(block, 0x18);
            func_801E53CC(row);
        }
        func_801C8574(0x5e);
    }
    for (i = 0; i < 3; i++) {
        if (D_800625A0->party->ids[i] != 0xff) {
            func_801E8DA8(D_800625A0->party->ids[i], i * 2);
            gear = D_8006D8A0[D_800625A0->party->ids[i]].gear;
            if (gear != 0xff) {
                func_801E8DA8(gear + 11, (i + 3) * 2);
            } else {
                func_801E8DA8(0xff, (i + 3) * 2);
            }
        }
    }
    func_801E8474(8, D_801EA19C);
    func_801D29A8(1, 0);
    func_801D28FC();
}

/* Refresh the item/status panels of `slot` for `mode` over two frames. */
void func_801D2EC0(u8 slot, u8 mode) {
    func_801D7C3C(slot, mode);
    func_801D7CFC(slot, mode, D_8006F8E5[slot]);
    func_801C7BF4();
    func_801D8DE4(slot, 0, 0, mode);
    func_801C7BF4();
    func_801D8EA4(slot, 0, 0, mode);
}

/* Open the notice window (portrait 2) and show three lines of label text
 * from entry `message`: four line labels (pairs share one render buffer)
 * rendered into VRAM and laid out as quads. */
void func_801D2F4C(u8 message) {
    s32 x;
    MenuMark *mark;
    MenuLabelSlot *line;
    s32 i;

    func_801D397C(2, 0x7a, 0x96, 0xbc, 0x40, 1, 1, 4, 0);
    mark = D_800625A0->portraitMarks[2];
    while (mark->done == 0) {
        func_801C7BF4();
    }
    x = 0x84;
    for (i = 0; i < 4; i++) {
        void *block = func_80031BDC(0x80, 0);

        D_800625A0->blocks1DE0[i] = block;
        bzero(block, 0x80);
        if (!(i & 1)) {
            D_800625A0->blocks1DE0[i]->pixels = func_80031BDC(0x5ca, 0);
            D_800625A0->blocks1DE0[i]->rect.x = 0x140;
            D_800625A0->blocks1DE0[i]->rect.y = (i / 2) * 13 + 0x4e;
            D_800625A0->blocks1DE0[i]->rect.w = 0x3a;
            D_800625A0->blocks1DE0[i]->rect.h = 13;
        } else {
            D_800625A0->blocks1DE0[i]->pixels = D_800625A0->blocks1DE0[i - 1]->pixels;
        }
    }
    for (i = 0; i < 3; i++) {
        line = D_800625A0->blocks1DE0[i];
        line->width = func_80034EAC(func_80033728(D_800625A0->labels, message + i), line->pixels, 0x36, i % 2);
        func_801E7C50(line, i, 0, 0);
        func_801E920C(&line->polys[D_800625A0->bufferIndex], (u16)x, (u16)(i * 16 + 0xa0), 0,
                      (u8)((i / 2) * 13 + 0x4e), line->width, 13);
        func_801C851C(line->verts, x, i * 16 + 0xa0, line->width, 13);
        line->count = D_800625A0->bufferIndex;
        line->visible = 1;
    }
    LoadImage(&D_800625A0->blocks1DE0[0]->rect, D_800625A0->blocks1DE0[0]->pixels);
    LoadImage(&D_800625A0->blocks1DE0[2]->rect, D_800625A0->blocks1DE0[2]->pixels);
    DrawSync(0);
    D_800625A0->party->unk2E = 1;
    func_800320E8(D_800625A0->blocks1DE0[0]->pixels);
    func_800320E8(D_800625A0->blocks1DE0[2]->pixels);
    func_801C7BF4();
    func_801C7BF4();
}

/* Close the notice when open (party +22): free portrait 2 and its four
 * blocks (+1de0), then finish a frame. */
void func_801D32B4(void) {
    s32 i;

    if (D_800625A0->party->unk20[2] != 0) {
        func_801D4EA0(2);
        D_800625A0->party->unk2E = 0;
        for (i = 0; i < 4; i++) {
            func_800320E8(D_800625A0->blocks1DE0[i]);
        }
    }
    func_801C7BF4();
}

/* Show the one-quad sprite (+43c; sheet image 107) at (x, y), `h` high. */
void func_801D3344(s32 x, s32 y, s32 h) {
    void *block;

    if (D_800625A0->party->unk49 == 0) {
        block = func_80031BDC(0x74, 0);
        D_800625A0->block43C = block;
        bzero(block, 0x74);
    }
    func_8002675C(D_800625A0->sheet, 0x107, D_800625A0->block43C, D_800625A0->bufferIndex, x, y, 0x1000);
    func_801C851C(D_800625A0->block43C->verts, x, y, 8, h);
    D_800625A0->block43C->buffer = D_800625A0->bufferIndex;
    D_800625A0->party->unk49 = 1;
}

/* Clear party flag +49 and free the block at state +43c. */
void func_801D3444(void) {
    D_800625A0->party->unk49 = 0;
    func_800320E8(D_800625A0->block43C);
}

/* Show the two party window sprites (sheet 164, 165) at `row` and lay out
 * their quads, unless only one member is listed (+32b, or +33b when
 * `fighters`). The block at +440 is allocated on first use. */
void func_801D3488(u8 row, u8 fighters) {
    s32 count;
    s32 i;
    void *block;

    if (!fighters) {
        count = D_800625A0->partyCount;
    } else {
        count = D_800625A0->fighters;
    }
    if (count != 1) {
        if (D_800625A0->party->unk67 == 0) {
            block = func_80031BDC(0x1c4, 0);
            D_800625A0->block440 = block;
            bzero(block, 0x1c4);
            D_800625A0->party->unk67 = 1;
        }
        for (i = 0; i < 2; i++) {
            func_8002675C(D_800625A0->sheet, 0x164 + i, &D_800625A0->block440->polys[i * 4], D_800625A0->bufferIndex,
                          D_801EA164[i], D_801EA16C[row], 0x1000);
        }
        for (i = 0; i < 4; i++) {
            func_801C851C(&D_800625A0->block440->verts[i * 4],
                          D_800625A0->block440->polys[i * 2 + D_800625A0->bufferIndex].x0,
                          D_800625A0->block440->polys[i * 2 + D_800625A0->bufferIndex].y0,
                          D_800625A0->block440->polys[i * 2 + D_800625A0->bufferIndex].x1 -
                              D_800625A0->block440->polys[i * 2 + D_800625A0->bufferIndex].x0,
                          D_800625A0->block440->polys[i * 2 + D_800625A0->bufferIndex].y3 -
                              D_800625A0->block440->polys[i * 2 + D_800625A0->bufferIndex].y0);
        }
        D_800625A0->block440->buffer = D_800625A0->bufferIndex;
        D_800625A0->party->unk53 = 1;
    }
}

/* Close the block at +440 when it is open (party +67), hiding its sprites. */
void func_801D3674(void) {
    MenuParty *party;

    party = D_800625A0->party;
    if (party->unk67 != 0) {
        party->unk53 = 0;
        D_800625A0->party->unk67 = 0;
        func_800320E8(D_800625A0->block440);
    }
}

/* Lay out `label` as the portrait of party slot `slot` (its character, or
 * its gear when `gear`) at the panel position of `mode`. */
void func_801D36E0(MenuLabelSlot *label, u8 slot, u8 gear, u8 mode) {
    s32 x;
    s32 w;

    func_801E927C(&label->polys[D_800625A0->bufferIndex]);
    label->polys[D_800625A0->bufferIndex].tpage = GetTPage(0, 0, 0x180, 0);
    if (!gear) {
        w = 0x48;
        label->polys[D_800625A0->bufferIndex].clut = (D_800625A0->party->ids[slot] & 1) ? D_80059414 : D_800595D4;
        x = D_801EA17C[mode] - 0x24;
        func_801E920C(&label->polys[D_800625A0->bufferIndex], (u16)x, (u16)D_801EA18C[mode],
                      (u8)(D_801EA578[slot] * 4), (u8)D_801EA5C4[slot], w, 13);
    } else {
        w = 0x60;
        label->polys[D_800625A0->bufferIndex].clut =
            ((D_8006D8A0[D_800625A0->party->ids[slot]].gear + 11) & 1) ? D_80059414 : D_800595D4;
        x = D_801EA17C[mode] - 0x30;
        func_801E920C(&label->polys[D_800625A0->bufferIndex], (u16)x, (u16)D_801EA18C[mode],
                      (u8)(D_801EA578[slot + 3] * 4), (u8)D_801EA5C4[slot + 3], w, 13);
    }
    func_801C851C(label->verts, x, D_801EA18C[mode], w, 13);
    label->count = D_800625A0->bufferIndex;
}

/* Open portrait window `index` at (x, y) of w x h: grow it in (`grow`) or
 * lay it out at once. Windows from 2 get their own blocks. */
void func_801D397C(u8 index, u16 x, u16 y, u16 w, u16 h, u8 grow, u8 arg6, s32 arg7, u8 arg8) {
    MenuMark *mark;
    void *block;

    if (index >= 2) {
        block = func_80031BDC(0x720, 0);
        D_800625A0->portraits[index] = block;
        bzero(block, 0x720);
        block = func_80031BDC(0x18, 0);
        D_800625A0->portraitMarks[index] = block;
        bzero(block, 0x18);
        func_801E53CC(index);
    }
    mark = D_800625A0->portraitMarks[index];
    if (grow) {
        mark->image = index;
        mark->done = 0;
        mark->x = x;
        mark->y = y;
        mark->w = w;
        mark->h = h;
        mark->curW = 0;
        mark->curH = 0;
        D_800625A0->party->unk27[index] = 1;
        mark->unk12 = arg6;
        mark->unkC = arg7;
        return;
    }
    func_801D4D1C(index, x, y, w, h, arg6, arg7, arg8);
}

/* Grow each shown portrait mark by 32 per frame towards its full size
 * (centred), marking it done when both sides are full, and draw it. */
void func_801D3B00(void) {
    s32 i;
    MenuMark *mark;
    u8 full;

    for (i = 0; i < 7; i++) {
        mark = D_800625A0->portraitMarks[i];
        if (D_800625A0->party->unk27[i] != 0 && mark->done == 0) {
            full = 0;
            if (mark->curW + 0x20 >= mark->w) {
                mark->curW = mark->w;
                full = 1;
            } else {
                mark->curW = mark->curW + 0x20;
            }
            if (mark->curH + 0x20 >= mark->h) {
                mark->curH = mark->h;
                full++;
            } else {
                mark->curH = mark->curH + 0x20;
            }
            if (full == 2) {
                mark->done = 1;
            }
            func_801D4D1C(mark->image, mark->x + (mark->w >> 1) - (mark->curW >> 1),
                          mark->y + (mark->h >> 1) - (mark->curH >> 1), mark->curW, mark->curH, mark->unk12,
                          mark->unkC, mark->unk13);
        }
    }
}

/* Lay out portrait `slot`'s frame at (x, y), `h` high: the top, the flipped
 * bottom and the side pieces. */
void func_801D3C4C(u8 slot, u16 x, u16 y, s32 unused, u16 h) {
    MenuPortrait *portrait;

    portrait = D_800625A0->portraits[slot];
    func_8002675C(D_800625A0->sheet, 0x105, portrait->frameEnds, D_800625A0->bufferIndex, x, y, 0x1000);
    func_800263E4(D_800625A0->sheet, 0x105, &portrait->frameEnds[2], D_800625A0->bufferIndex, x, y + h - 8, 0x1000,
                  0, 1);
    func_8002675C(D_800625A0->sheet, 0x106, portrait->frameSide, D_800625A0->bufferIndex, x, y + 8, 0x1000);
    func_801C851C(&portrait->endsAt[0], x, y, 8, 8);
    func_801C851C(&portrait->endsAt[4], x, y + h, 8, -8);
    func_801C851C(portrait->sideAt, x, y + 8, 8, h - 8);
}

/* Build portrait window `index`'s four corner sprites (sheet fd, ff, 102,
 * 104) for this buffer and place them around the w x h rectangle at (x, y),
 * mirrored by negative extents; make them semi-transparent. */
void func_801D3DB0(u8 index, u16 x, u16 y, u16 w, u16 h) {
    MenuPortrait *portrait = D_800625A0->portraits[index];
    s32 i;

    portrait->cornerParts = 0;
    portrait->cornerParts += func_8002675C(D_800625A0->sheet, 0xfd, portrait->corner, D_800625A0->bufferIndex, 0, 0,
                                           0x1000);
    portrait->cornerParts += func_8002675C(D_800625A0->sheet, 0xff, &portrait->corner[portrait->cornerParts * 2],
                                           D_800625A0->bufferIndex, 0, 0, 0x1000);
    portrait->cornerParts += func_8002675C(D_800625A0->sheet, 0x102, &portrait->corner[portrait->cornerParts * 2],
                                           D_800625A0->bufferIndex, 0, 0, 0x1000);
    portrait->cornerParts += func_8002675C(D_800625A0->sheet, 0x104, &portrait->corner[portrait->cornerParts * 2],
                                           D_800625A0->bufferIndex, 0, 0, 0x1000);
    func_801C851C(&portrait->cornerAt[0], x - 8, y + 8, 16, -16);
    func_801C851C(&portrait->cornerAt[4], x + w + 8, y + 8, -16, -16);
    func_801C851C(&portrait->cornerAt[8], x - 8, y + h - 8, 16, 16);
    func_801C851C(&portrait->cornerAt[12], x + w + 8, y + h - 8, -16, 16);
    for (i = 0; i < 4; i++) {
        func_801E91C4(&portrait->corner[i * 2 + D_800625A0->bufferIndex]);
    }
}

/* Map portrait window `index`'s top edge pieces for this buffer and place them in two
 * halves along the top of (x, y, w). */
void func_801D3FF8(u8 index, u16 x, u16 y, u16 w) {
    MenuPortrait *portrait = D_800625A0->portraits[index];
    s32 half;
    s32 i;

    (portrait->edge[0] + D_800625A0->bufferIndex)->u0 = 0;
    (portrait->edge[0] + D_800625A0->bufferIndex)->v0 = 0x84;
    (portrait->edge[0] + D_800625A0->bufferIndex)->u1 = 7;
    (portrait->edge[0] + D_800625A0->bufferIndex)->v1 = 0x84;
    (portrait->edge[0] + D_800625A0->bufferIndex)->u2 = 0;
    (portrait->edge[0] + D_800625A0->bufferIndex)->v2 = 0x94;
    (portrait->edge[0] + D_800625A0->bufferIndex)->u3 = 7;
    (portrait->edge[0] + D_800625A0->bufferIndex)->v3 = 0x94;
    (portrait->edge[0] + D_800625A0->bufferIndex + 2)->u0 = 0;
    (portrait->edge[0] + D_800625A0->bufferIndex + 2)->v0 = 0x84;
    (portrait->edge[0] + D_800625A0->bufferIndex + 2)->u1 = 7;
    (portrait->edge[0] + D_800625A0->bufferIndex + 2)->v1 = 0x84;
    (portrait->edge[0] + D_800625A0->bufferIndex + 2)->u2 = 0;
    (portrait->edge[0] + D_800625A0->bufferIndex + 2)->v2 = 0x94;
    (portrait->edge[0] + D_800625A0->bufferIndex + 2)->u3 = 7;
    (portrait->edge[0] + D_800625A0->bufferIndex + 2)->v3 = 0x94;
    half = (w - 16) / 2;
    func_801C851C(portrait->edgeAt[0][0], x + 8, y - 8, half, 16);
    func_801C851C(portrait->edgeAt[0][1], x + (half + 8), y - 8, half, 16);
    for (i = 0; i < 2; i++) {
        func_801E91C4(&portrait->edge[0][i * 2 + D_800625A0->bufferIndex]);
    }
}

/* Map portrait window `index`'s bottom edge pieces for this buffer and place them in
 * two halves along the bottom of (x, y, w, h). */
void func_801D433C(u8 index, u16 x, u16 y, u16 w, u16 h) {
    MenuPortrait *portrait = D_800625A0->portraits[index];
    s32 half;
    s32 i;

    (portrait->edge[1] + D_800625A0->bufferIndex)->u0 = 8;
    (portrait->edge[1] + D_800625A0->bufferIndex)->v0 = 0x84;
    (portrait->edge[1] + D_800625A0->bufferIndex)->u1 = 0xF;
    (portrait->edge[1] + D_800625A0->bufferIndex)->v1 = 0x84;
    (portrait->edge[1] + D_800625A0->bufferIndex)->u2 = 8;
    (portrait->edge[1] + D_800625A0->bufferIndex)->v2 = 0x94;
    (portrait->edge[1] + D_800625A0->bufferIndex)->u3 = 0xF;
    (portrait->edge[1] + D_800625A0->bufferIndex)->v3 = 0x94;
    (portrait->edge[1] + D_800625A0->bufferIndex + 2)->u0 = 8;
    (portrait->edge[1] + D_800625A0->bufferIndex + 2)->v0 = 0x84;
    (portrait->edge[1] + D_800625A0->bufferIndex + 2)->u1 = 0xF;
    (portrait->edge[1] + D_800625A0->bufferIndex + 2)->v1 = 0x84;
    (portrait->edge[1] + D_800625A0->bufferIndex + 2)->u2 = 8;
    (portrait->edge[1] + D_800625A0->bufferIndex + 2)->v2 = 0x94;
    (portrait->edge[1] + D_800625A0->bufferIndex + 2)->u3 = 0xF;
    (portrait->edge[1] + D_800625A0->bufferIndex + 2)->v3 = 0x94;
    half = (w - 16) / 2;
    func_801C851C(portrait->edgeAt[1][0], x + 8, y + h - 8, half, 16);
    func_801C851C(portrait->edgeAt[1][1], x + (half + 8), y + h - 8, half, 16);
    for (i = 0; i < 2; i++) {
        func_801E91C4(&portrait->edge[1][i * 2 + D_800625A0->bufferIndex]);
    }
}

/* Map portrait window `index`'s left edge pieces for this buffer and place them in two
 * halves down the left of (x, y, h). */
void func_801D4688(u8 index, u16 x, u16 y, u16 h) {
    MenuPortrait *portrait = D_800625A0->portraits[index];
    s32 half;
    s32 i;

    (portrait->edge[2] + D_800625A0->bufferIndex)->u0 = 0x10;
    (portrait->edge[2] + D_800625A0->bufferIndex)->v0 = 0x84;
    (portrait->edge[2] + D_800625A0->bufferIndex)->u1 = 0x20;
    (portrait->edge[2] + D_800625A0->bufferIndex)->v1 = 0x84;
    (portrait->edge[2] + D_800625A0->bufferIndex)->u2 = 0x10;
    (portrait->edge[2] + D_800625A0->bufferIndex)->v2 = 0x8B;
    (portrait->edge[2] + D_800625A0->bufferIndex)->u3 = 0x20;
    (portrait->edge[2] + D_800625A0->bufferIndex)->v3 = 0x8B;
    (portrait->edge[2] + D_800625A0->bufferIndex + 2)->u0 = 0x10;
    (portrait->edge[2] + D_800625A0->bufferIndex + 2)->v0 = 0x84;
    (portrait->edge[2] + D_800625A0->bufferIndex + 2)->u1 = 0x20;
    (portrait->edge[2] + D_800625A0->bufferIndex + 2)->v1 = 0x84;
    (portrait->edge[2] + D_800625A0->bufferIndex + 2)->u2 = 0x10;
    (portrait->edge[2] + D_800625A0->bufferIndex + 2)->v2 = 0x8B;
    (portrait->edge[2] + D_800625A0->bufferIndex + 2)->u3 = 0x20;
    (portrait->edge[2] + D_800625A0->bufferIndex + 2)->v3 = 0x8B;
    half = (h - 16) / 2;
    func_801C851C(portrait->edgeAt[2][0], x - 8, y + 8, 16, half);
    func_801C851C(portrait->edgeAt[2][1], x - 8, y + (half + 8), 16, half);
    for (i = 0; i < 2; i++) {
        func_801E91C4(&portrait->edge[2][i * 2 + D_800625A0->bufferIndex]);
    }
}

/* Map portrait window `index`'s right edge pieces for this buffer and place them in two
 * halves down the right of (x, y, w, h). */
void func_801D49D0(u8 index, u16 x, u16 y, u16 w, u16 h) {
    MenuPortrait *portrait = D_800625A0->portraits[index];
    s32 half;
    s32 i;

    (portrait->edge[3] + D_800625A0->bufferIndex)->u0 = 0x10;
    (portrait->edge[3] + D_800625A0->bufferIndex)->v0 = 0x8C;
    (portrait->edge[3] + D_800625A0->bufferIndex)->u1 = 0x20;
    (portrait->edge[3] + D_800625A0->bufferIndex)->v1 = 0x8C;
    (portrait->edge[3] + D_800625A0->bufferIndex)->u2 = 0x10;
    (portrait->edge[3] + D_800625A0->bufferIndex)->v2 = 0x93;
    (portrait->edge[3] + D_800625A0->bufferIndex)->u3 = 0x20;
    (portrait->edge[3] + D_800625A0->bufferIndex)->v3 = 0x93;
    (portrait->edge[3] + D_800625A0->bufferIndex + 2)->u0 = 0x10;
    (portrait->edge[3] + D_800625A0->bufferIndex + 2)->v0 = 0x8C;
    (portrait->edge[3] + D_800625A0->bufferIndex + 2)->u1 = 0x20;
    (portrait->edge[3] + D_800625A0->bufferIndex + 2)->v1 = 0x8C;
    (portrait->edge[3] + D_800625A0->bufferIndex + 2)->u2 = 0x10;
    (portrait->edge[3] + D_800625A0->bufferIndex + 2)->v2 = 0x93;
    (portrait->edge[3] + D_800625A0->bufferIndex + 2)->u3 = 0x20;
    (portrait->edge[3] + D_800625A0->bufferIndex + 2)->v3 = 0x93;
    half = (h - 16) / 2;
    func_801C851C(portrait->edgeAt[3][0], x + w - 8, y + 8, 16, half);
    func_801C851C(portrait->edgeAt[3][1], x + w - 8, y + (half + 8), 16, half);
    for (i = 0; i < 2; i++) {
        func_801E91C4(&portrait->edge[3][i * 2 + D_800625A0->bufferIndex]);
    }
}

/* Lay out portrait window `index` at (x, y) of w x h and show it. */
void func_801D4D1C(u8 index, u16 x, u16 y, u16 w, u16 h, u8 style, s32 depth, u8 framed) {
    MenuPortrait *portrait;

    portrait = D_800625A0->portraits[index];
    D_800625A0->party->unk20[index] = 0;
    func_801C851C(portrait->fillAt, x, y, w, h);
    func_801D3DB0(index, x, y, w, h);
    func_801D3FF8(index, x, y, w);
    func_801D433C(index, x, y, w, h);
    func_801D4688(index, x, y, h);
    func_801D49D0(index, x, y, w, h);
    if (framed) {
        func_801D3C4C(index, x, y, w, h);
    }
    portrait->framed = framed;
    portrait->style = style;
    portrait->depth = depth;
    portrait->buffer = D_800625A0->bufferIndex;
    D_800625A0->party->unk20[index] = 1;
}

/* Hide and free portrait `slot`. */
void func_801D4EA0(u8 slot) {
    D_800625A0->party->unk20[slot] = 0;
    D_800625A0->party->unk27[slot] = 0;
    func_800320E8(D_800625A0->portraits[slot]);
    func_800320E8(D_800625A0->portraitMarks[slot]);
}

/* Build field block `index`'s frame (sheet 14b + index) at (x, y) and its
 * character portrait quad. */
void func_801D4F2C(u8 index, u8 mode, s32 x, s32 y) {
    MenuFieldBlock *block = D_800625A0->fieldBlocks[index];

    func_8002675C(D_800625A0->sheet, index + 0x14b, block->frameA, D_800625A0->bufferIndex, x, y, 0x1000);
    func_801E927C(&block->frameB[D_800625A0->bufferIndex]);
    block->frameB[D_800625A0->bufferIndex].tpage = GetTPage(0, 0, 0x180, 0);
    block->frameB[D_800625A0->bufferIndex].clut = (D_800625A0->party->ids[index] & 1) ? D_80059414 : D_800595D4;
    func_801E920C(&block->frameB[D_800625A0->bufferIndex], (u16)(D_801E9B58 + x), (u16)(D_801E9B5C + y),
                  (u8)(D_801EA578[index] * 4), (u8)D_801EA5C4[index], 0x48, 13);
}

/* Lay out the parts of field block `index` at (x, y) from the D_801EA34C
 * sheet images (ffff none). */
void func_801D50EC(u8 index, s32 x, s32 y) {
    MenuFieldBlock *block;
    s32 i;

    block = D_800625A0->fieldBlocks[index];
    block->count0 = 0;
    for (i = 0; i < 20; i++) {
        if (D_801EA34C[i] != 0xffff) {
            block->count0 += func_8002675C(D_800625A0->sheet, D_801EA34C[i], &block->list0[block->count0 * 2],
                                           D_800625A0->bufferIndex, x + D_801E9A78[i], y + D_801E9AC8[i],
                                           0x1000);
        }
    }
}

/* Lay out character `ch`'s hp (by digit position) and hp maximum (packed)
 * on field block `index` at (x, y). */
void func_801D51EC(u8 index, u8 ch, s32 x, s32 y) {
    MenuFieldBlock *block = D_800625A0->fieldBlocks[index];
    s32 i;
    s32 n;
    u8 digit;

    func_801C80B8(D_8006D8A0[ch].hp);
    block->count1 = 0;
    for (i = 0; i < 3; i++) {
        digit = D_800625A0->digits[6 + i];
        if (digit != 0xff) {
            block->count1 += func_8002675C(D_800625A0->sheet, digit, &block->list1[block->count1 * 2],
                                           D_800625A0->bufferIndex, x + D_801E9B28 + i * 8, y + D_801E9B2C, 0x1000);
        }
    }
    func_801C80B8(D_8006D8A0[ch].hpMax);
    block->count2 = 0;
    for (i = 0, n = 0; i < 3; i++) {
        digit = D_800625A0->digits[6 + i];
        if (digit != 0xff) {
            block->count2 += func_8002675C(D_800625A0->sheet, digit, &block->list2[block->count2 * 2],
                                           D_800625A0->bufferIndex, x + D_801E9B30 + n * 8, y + D_801E9B34, 0x1000);
            n++;
        }
    }
}

/* Lay out character `ch`'s ep (by digit position) and ep maximum (packed)
 * on field block `index` at (x, y). */
void func_801D53D0(u8 index, u8 ch, s32 x, s32 y) {
    MenuFieldBlock *block = D_800625A0->fieldBlocks[index];
    s32 i;
    s32 n;
    u8 digit;

    func_801C80B8(D_8006D8A0[ch].ep);
    block->count3 = 0;
    for (i = 0; i < 2; i++) {
        digit = D_800625A0->digits[7 + i];
        if (digit != 0xff) {
            block->count3 += func_8002675C(D_800625A0->sheet, digit, &block->list3[block->count3 * 2],
                                           D_800625A0->bufferIndex, x + D_801E9B38 + i * 8, y + D_801E9B3C, 0x1000);
        }
    }
    func_801C80B8(D_8006D8A0[ch].epMax);
    block->count4 = 0;
    for (i = 0, n = 0; i < 2; i++) {
        digit = D_800625A0->digits[7 + i];
        if (digit != 0xff) {
            block->count4 += func_8002675C(D_800625A0->sheet, digit, &block->list4[block->count4 * 2],
                                           D_800625A0->bufferIndex, x + D_801E9B40 + n * 8, y + D_801E9B44, 0x1000);
            n++;
        }
    }
}

/* Lay out character `ch`'s experience and experience to the next level
 * (seven digits each, by digit position) on field block `index` at (x, y). */
void func_801D55B4(u8 index, u8 ch, s32 x, s32 y) {
    MenuFieldBlock *block = D_800625A0->fieldBlocks[index];
    s32 i;
    u8 digit;

    func_801C80B8(D_8006D8A0[ch].exp);
    block->count5 = 0;
    for (i = 0; i < 7; i++) {
        digit = D_800625A0->digits[2 + i];
        if (digit != 0xff) {
            block->count5 += func_8002675C(D_800625A0->sheet, digit, &block->list5[block->count5 * 2],
                                           D_800625A0->bufferIndex, x + D_801E9B48 + i * 8, y + D_801E9B4C, 0x1000);
        }
    }
    func_801C80B8(D_8006D8A0[ch].expNext);
    block->count7 = 0;
    for (i = 0; i < 7; i++) {
        digit = D_800625A0->digits[2 + i];
        if (digit != 0xff) {
            block->count7 += func_8002675C(D_800625A0->sheet, digit, &block->list7[block->count7 * 2],
                                           D_800625A0->bufferIndex, x + D_801E9B50 + i * 8, y + D_801E9B54, 0x1000);
        }
    }
}

/* Lay out character `ch`'s +62 and +63 values (three digits each, by digit
 * position) on field block `index` at (x, y), the second tinted green. */
void func_801D5794(u8 index, u8 ch, s32 x, s32 y) {
    MenuFieldBlock *block = D_800625A0->fieldBlocks[index];
    s32 i;
    u8 digit;

    func_801C80B8(D_8006D8A0[ch].unk62);
    block->count6 = 0;
    for (i = 0; i < 3; i++) {
        digit = D_800625A0->digits[6 + i];
        if (digit != 0xff) {
            block->count6 += func_8002675C(D_800625A0->sheet, digit, &block->list6[block->count6 * 2],
                                           D_800625A0->bufferIndex, x + D_801E9B18 + i * 8, y + D_801E9B1C, 0x1000);
        }
    }
    func_801C80B8(D_8006D8A0[ch].unk63);
    block->count8 = 0;
    for (i = 0; i < 3; i++) {
        digit = D_800625A0->digits[6 + i];
        if (digit != 0xff) {
            block->count8 += func_8002675C(D_800625A0->sheet, digit, &block->list8[block->count8 * 2],
                                           D_800625A0->bufferIndex, x + D_801E9B20 + i * 8, y + D_801E9B24, 0x1000);
        }
    }
    for (i = 0; i < block->count8; i++) {
        SetShadeTex(&block->list8[i * 2 + D_800625A0->bufferIndex], 0);
        (block->list8 + (i * 2 + D_800625A0->bufferIndex))->r0 = 0;
        (block->list8 + (i * 2 + D_800625A0->bufferIndex))->g0 = 0x80;
        (block->list8 + (i * 2 + D_800625A0->bufferIndex))->b0 = 0;
    }
}

/* Lay out field block `index` (none for ff) at its mover's position in
 * `mode` and show it. */
void func_801D5A50(u8 index, u8 mode) {
    MenuFieldBlock *block;
    s32 x;
    s32 y;

    if (index != 0xff) {
        block = D_800625A0->fieldBlocks[index];
        x = D_800625A0->movers[index].accX / 256 + D_800625A0->movers[index].x0;
        y = D_800625A0->movers[index].accY / 256 + D_800625A0->movers[index].y0;
        func_801D4F2C(index, mode, x, y);
        func_801D50EC(index, x, y);
        func_801D51EC(index, mode, x, y);
        func_801D53D0(index, mode, x, y);
        func_801D55B4(index, mode, x, y);
        func_801D5794(index, mode, x, y);
        D_800625A0->party->fieldShown[index] = 1;
        block->buffer = D_800625A0->bufferIndex;
    }
}

/* Lay out the money (D_8006EF58) digits at (x, y) and its unit mark. */
void func_801D5BA4(s32 x, s32 y) {
    s32 i;
    s32 dx;
    u8 digit;

    func_801C80B8(D_8006EF58);
    i = 0;
    dx = x;
    D_800625A0->fieldMenu->count = 0;
    for (; i < 9; i++) {
        digit = D_800625A0->digits[i];
        if (digit != 0xff) {
            D_800625A0->fieldMenu->count +=
                func_8002675C(D_800625A0->sheet, digit, &D_800625A0->fieldMenu->polys[D_800625A0->fieldMenu->count * 2],
                              D_800625A0->bufferIndex, dx, y, 0x1000);
        }
        dx += 8;
    }
    func_8002675C(D_800625A0->sheet, 0x10, D_800625A0->fieldMenu->cursor, D_800625A0->bufferIndex, x + 0x50, y,
                  0x1000);
    D_800625A0->party->redraw5 = 1;
    D_800625A0->fieldMenu->start = D_800625A0->bufferIndex;
}

/* Lay out the play time digits (hours, minutes, seconds) at (x, y) with
 * their two separators. */
void func_801D5CF8(s32 x, s32 y) {
    s32 i;

    for (i = 0; i < 3; i++) {
        func_8002675C(D_800625A0->sheet, D_800625A0->time[i], &D_800625A0->fieldMenu2->polys[i * 2],
                      D_800625A0->bufferIndex, x + i * 8, y, 0x1000);
    }
    for (i = 3; i < 5; i++) {
        func_8002675C(D_800625A0->sheet, D_800625A0->time[i], &D_800625A0->fieldMenu2->polys[i * 2],
                      D_800625A0->bufferIndex, x + i * 8 + 8, y, 0x1000);
    }
    for (i = 5; i < 7; i++) {
        func_8002675C(D_800625A0->sheet, D_800625A0->time[i], &D_800625A0->fieldMenu2->polys[i * 2],
                      D_800625A0->bufferIndex, x + i * 8 + 0x10, y, 0x1000);
    }
    func_8002675C(D_800625A0->sheet, 0xee, &D_800625A0->fieldMenu2->polys2[0], D_800625A0->bufferIndex, x + 0x18, y,
                  0x1000);
    func_8002675C(D_800625A0->sheet, 0xee, &D_800625A0->fieldMenu2->polys2[4], D_800625A0->bufferIndex, x + 0x30, y,
                  0x1000);
    D_800625A0->fieldMenu2->start = D_800625A0->bufferIndex;
}

/* Lay out the detail panel's frame (sheet 14b + slot) and the portrait of
 * party slot `slot`: its character, or its gear (further left) when `gear`. */
void func_801D5ED4(u8 slot, u8 gear) {
    s32 shift;
    s32 w;

    shift = gear ? 0x18 : 0;
    func_8002675C(D_800625A0->sheet, slot + 0x14b, D_800625A0->block358->frame, D_800625A0->bufferIndex,
                  0x18 - shift, 0xe, 0x1000);
    func_801C851C(D_800625A0->block358->frameAt, D_800625A0->block358->frame[D_800625A0->bufferIndex].x0,
                  D_800625A0->block358->frame[D_800625A0->bufferIndex].y0, 0x30, 0x30);
    func_801E927C(&D_800625A0->block358->portrait[D_800625A0->bufferIndex]);
    D_800625A0->block358->portrait[D_800625A0->bufferIndex].tpage = GetTPage(0, 0, 0x180, 0);
    if (!gear) {
        D_800625A0->block358->portrait[D_800625A0->bufferIndex].clut =
            (D_800625A0->party->ids[slot] & 1) ? D_80059414 : D_800595D4;
    } else {
        D_800625A0->block358->portrait[D_800625A0->bufferIndex].clut =
            ((D_8006D8A0[D_800625A0->party->ids[slot]].gear + 11) & 1) ? D_80059414 : D_800595D4;
    }
    w = gear * 0x18 + 0x48;
    func_801E920C(&D_800625A0->block358->portrait[D_800625A0->bufferIndex], 0, 0,
                  (u8)(D_801EA578[gear * 3 + slot] * 4), (u8)D_801EA5C4[gear * 3 + slot], (u16)w, 13);
    func_801C851C(D_800625A0->block358->portraitAt, D_801E9D38 - shift, D_801E9D3C, w, 13);
}

/* Build the detail panel's parts of `layout` from the sheet and place each
 * part's quad where the sheet put it. */
void func_801D6194(u8 layout) {
    s32 i;

    D_800625A0->block358->count = 0;
    for (i = 0; i < 24; i++) {
        if (D_801EA39C[layout * 24 + i] != 0xffff) {
            D_800625A0->block358->count +=
                func_8002675C(D_800625A0->sheet, D_801EA39C[layout * 24 + i],
                              &D_800625A0->block358->parts[D_800625A0->block358->count * 2], D_800625A0->bufferIndex,
                              D_801E9B60[layout * 24 + i], D_801E9C20[layout * 24 + i], 0x1000);
        }
    }
    for (i = 0; i < D_800625A0->block358->count; i++) {
        func_801C851C(D_800625A0->block358->partsAt[i],
                      D_800625A0->block358->parts[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->parts[i * 2 + D_800625A0->bufferIndex].y0,
                      D_800625A0->block358->parts[i * 2 + D_800625A0->bufferIndex].x1 -
                          D_800625A0->block358->parts[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->parts[i * 2 + D_800625A0->bufferIndex].y3 -
                          D_800625A0->block358->parts[i * 2 + D_800625A0->bufferIndex].y0);
    }
}

/* Lay out the detail panel's hp (by digit position) and hp maximum (packed)
 * of party slot `slot`: the character's (three digits) or, with `gear`, its
 * gear's (five digits). */
void func_801D6338(u8 slot, u8 gear) {
    s32 digits;
    s32 first;
    s32 x;
    s32 i;
    u8 digit;

    if (!gear) {
        digits = 3;
        first = 6;
        x = D_801E9CF0;
        func_801C80B8(D_8006D8A0[D_800625A0->party->ids[slot]].hp);
    } else {
        digits = 5;
        first = 4;
        x = D_801E9CF0 + 8;
        func_801C80B8(D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unk60);
    }
    D_800625A0->block358->hpCount = 0;
    for (i = 0; i < digits; i++) {
        digit = D_800625A0->digits[first + i];
        if (digit != 0xff) {
            D_800625A0->block358->hpCount +=
                func_8002675C(D_800625A0->sheet, digit, &D_800625A0->block358->hp[D_800625A0->block358->hpCount * 2],
                              D_800625A0->bufferIndex, x - gear * 0x18, D_801E9CF4, 0x1000);
        }
        x += 8;
    }
    for (i = 0; i < D_800625A0->block358->hpCount; i++) {
        func_801C851C(D_800625A0->block358->hpAt[i], D_800625A0->block358->hp[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->hp[i * 2 + D_800625A0->bufferIndex].y0,
                      D_800625A0->block358->hp[i * 2 + D_800625A0->bufferIndex].x1 -
                          D_800625A0->block358->hp[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->hp[i * 2 + D_800625A0->bufferIndex].y3 -
                          D_800625A0->block358->hp[i * 2 + D_800625A0->bufferIndex].y0);
    }
    if (!gear) {
        func_801C80B8(D_8006D8A0[D_800625A0->party->ids[slot]].hpMax);
        x = D_801E9CF8;
    } else {
        func_801C80B8(D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unk64);
        x = D_801E9CF8 - 0x20;
    }
    D_800625A0->block358->hpMaxCount = 0;
    for (i = 0; i < digits; i++) {
        digit = D_800625A0->digits[first + i];
        if (digit != 0xff) {
            D_800625A0->block358->hpMaxCount +=
                func_8002675C(D_800625A0->sheet, digit,
                              &D_800625A0->block358->hpMax[D_800625A0->block358->hpMaxCount * 2],
                              D_800625A0->bufferIndex, x - gear * 0x18, gear * 8 + D_801E9CFC, 0x1000);
            x += 8;
        }
    }
    for (i = 0; i < D_800625A0->block358->hpMaxCount; i++) {
        func_801C851C(D_800625A0->block358->hpMaxAt[i],
                      D_800625A0->block358->hpMax[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->hpMax[i * 2 + D_800625A0->bufferIndex].y0,
                      D_800625A0->block358->hpMax[i * 2 + D_800625A0->bufferIndex].x1 -
                          D_800625A0->block358->hpMax[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->hpMax[i * 2 + D_800625A0->bufferIndex].y3 -
                          D_800625A0->block358->hpMax[i * 2 + D_800625A0->bufferIndex].y0);
    }
}

/* Lay out the detail panel's ep (by digit position) and ep maximum (packed)
 * of party slot `slot`: the character's (two digits) or, with `gear`, its
 * gear's +38 and +3a (four digits). */
void func_801D680C(u8 slot, u8 gear) {
    s32 digits;
    s32 first;
    s32 x;
    s32 y;
    s32 i;
    u8 digit;

    if (!gear) {
        digits = 2;
        first = 7;
        x = D_801E9D00;
        y = D_801E9D04;
        func_801C80B8(D_8006D8A0[D_800625A0->party->ids[slot]].ep);
    } else {
        digits = 4;
        first = 5;
        x = D_801E9D00;
        y = D_801E9D04 + 8;
        func_801C80B8(D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unk38);
    }
    D_800625A0->block358->epCount = 0;
    for (i = 0; i < digits; i++) {
        digit = D_800625A0->digits[first + i];
        if (digit != 0xff) {
            D_800625A0->block358->epCount +=
                func_8002675C(D_800625A0->sheet, digit, &D_800625A0->block358->ep[D_800625A0->block358->epCount * 2],
                              D_800625A0->bufferIndex, x - gear * 0x18, y, 0x1000);
        }
        x += 8;
    }
    for (i = 0; i < D_800625A0->block358->epCount; i++) {
        func_801C851C(D_800625A0->block358->epAt[i], D_800625A0->block358->ep[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->ep[i * 2 + D_800625A0->bufferIndex].y0,
                      D_800625A0->block358->ep[i * 2 + D_800625A0->bufferIndex].x1 -
                          D_800625A0->block358->ep[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->ep[i * 2 + D_800625A0->bufferIndex].y3 -
                          D_800625A0->block358->ep[i * 2 + D_800625A0->bufferIndex].y0);
    }
    if (!gear) {
        func_801C80B8(D_8006D8A0[D_800625A0->party->ids[slot]].epMax);
        x = D_801E9D08;
        y = D_801E9D0C;
    } else {
        func_801C80B8(D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unk3A);
        x = D_801E9D08 - 0x20;
        y = D_801E9D0C + 0x10;
    }
    D_800625A0->block358->epMaxCount = 0;
    for (i = 0; i < digits; i++) {
        digit = D_800625A0->digits[first + i];
        if (digit != 0xff) {
            D_800625A0->block358->epMaxCount +=
                func_8002675C(D_800625A0->sheet, digit,
                              &D_800625A0->block358->epMax[D_800625A0->block358->epMaxCount * 2],
                              D_800625A0->bufferIndex, x - gear * 0x18, y, 0x1000);
            x += 8;
        }
    }
    for (i = 0; i < D_800625A0->block358->epMaxCount; i++) {
        func_801C851C(D_800625A0->block358->epMaxAt[i],
                      D_800625A0->block358->epMax[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->epMax[i * 2 + D_800625A0->bufferIndex].y0,
                      D_800625A0->block358->epMax[i * 2 + D_800625A0->bufferIndex].x1 -
                          D_800625A0->block358->epMax[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->epMax[i * 2 + D_800625A0->bufferIndex].y3 -
                          D_800625A0->block358->epMax[i * 2 + D_800625A0->bufferIndex].y0);
    }
}

/* Lay out the detail panel's level and +63 value (three digits each, by
 * digit position) of party slot `slot`, the second tinted green; `gear`
 * shifts them left. */
void func_801D6CF4(u8 slot, u8 gear) {
    s32 i;
    u8 digit;

    func_801C80B8(D_8006D8A0[D_800625A0->party->ids[slot]].unk62);
    D_800625A0->block358->levelCount = 0;
    for (i = 0; i < 3; i++) {
        digit = D_800625A0->digits[6 + i];
        if (digit != 0xff) {
            D_800625A0->block358->levelCount +=
                func_8002675C(D_800625A0->sheet, digit,
                              &D_800625A0->block358->level[D_800625A0->block358->levelCount * 2],
                              D_800625A0->bufferIndex, i * 8 + D_801E9CE0 - gear * 0x18, D_801E9CE4, 0x1000);
        }
    }
    for (i = 0; i < D_800625A0->block358->levelCount; i++) {
        func_801C851C(D_800625A0->block358->levelAt[i],
                      D_800625A0->block358->level[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->level[i * 2 + D_800625A0->bufferIndex].y0,
                      D_800625A0->block358->level[i * 2 + D_800625A0->bufferIndex].x1 -
                          D_800625A0->block358->level[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->level[i * 2 + D_800625A0->bufferIndex].y3 -
                          D_800625A0->block358->level[i * 2 + D_800625A0->bufferIndex].y0);
    }
    func_801C80B8(D_8006D8A0[D_800625A0->party->ids[slot]].unk63);
    D_800625A0->block358->level2Count = 0;
    for (i = 0; i < 3; i++) {
        digit = D_800625A0->digits[6 + i];
        if (digit != 0xff) {
            D_800625A0->block358->level2Count +=
                func_8002675C(D_800625A0->sheet, digit,
                              &D_800625A0->block358->level2[D_800625A0->block358->level2Count * 2],
                              D_800625A0->bufferIndex, i * 8 + D_801E9CE8 - gear * 0x18, D_801E9CEC, 0x1000);
        }
    }
    for (i = 0; i < D_800625A0->block358->level2Count; i++) {
        SetShadeTex(&D_800625A0->block358->level2[i * 2 + D_800625A0->bufferIndex], 0);
        (D_800625A0->block358->level2 + (i * 2 + D_800625A0->bufferIndex))->r0 = 0;
        (D_800625A0->block358->level2 + (i * 2 + D_800625A0->bufferIndex))->g0 = 0x80;
        (D_800625A0->block358->level2 + (i * 2 + D_800625A0->bufferIndex))->b0 = 0;
        func_801C851C(D_800625A0->block358->level2At[i],
                      D_800625A0->block358->level2[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->level2[i * 2 + D_800625A0->bufferIndex].y0,
                      D_800625A0->block358->level2[i * 2 + D_800625A0->bufferIndex].x1 -
                          D_800625A0->block358->level2[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->level2[i * 2 + D_800625A0->bufferIndex].y3 -
                          D_800625A0->block358->level2[i * 2 + D_800625A0->bufferIndex].y0);
    }
}

/* Lay out the detail panel's +3c and +40 values (eight digits each, by
 * digit position) of party slot `slot`; `gear` shifts them left. */
void func_801D7154(u8 slot, u8 gear) {
    s32 i;
    u8 digit;

    func_801C80B8(D_8006D8A0[D_800625A0->party->ids[slot]].unk3C);
    D_800625A0->block358->value3CCount = 0;
    for (i = 0; i < 8; i++) {
        digit = D_800625A0->digits[1 + i];
        if (digit != 0xff) {
            D_800625A0->block358->value3CCount +=
                func_8002675C(D_800625A0->sheet, digit,
                              &D_800625A0->block358->value3C[D_800625A0->block358->value3CCount * 2],
                              D_800625A0->bufferIndex, i * 8 + D_801E9D10 - gear * 0x18, D_801E9D14, 0x1000);
        }
    }
    for (i = 0; i < D_800625A0->block358->value3CCount; i++) {
        func_801C851C(D_800625A0->block358->value3CAt[i],
                      D_800625A0->block358->value3C[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->value3C[i * 2 + D_800625A0->bufferIndex].y0,
                      D_800625A0->block358->value3C[i * 2 + D_800625A0->bufferIndex].x1 -
                          D_800625A0->block358->value3C[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->value3C[i * 2 + D_800625A0->bufferIndex].y3 -
                          D_800625A0->block358->value3C[i * 2 + D_800625A0->bufferIndex].y0);
    }
    func_801C80B8(D_8006D8A0[D_800625A0->party->ids[slot]].unk40);
    D_800625A0->block358->value40Count = 0;
    for (i = 0; i < 8; i++) {
        digit = D_800625A0->digits[1 + i];
        if (digit != 0xff) {
            D_800625A0->block358->value40Count +=
                func_8002675C(D_800625A0->sheet, digit,
                              &D_800625A0->block358->value40[D_800625A0->block358->value40Count * 2],
                              D_800625A0->bufferIndex, i * 8 + D_801E9D18 - gear * 0x18, D_801E9D1C, 0x1000);
        }
    }
    for (i = 0; i < D_800625A0->block358->value40Count; i++) {
        func_801C851C(D_800625A0->block358->value40At[i],
                      D_800625A0->block358->value40[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->value40[i * 2 + D_800625A0->bufferIndex].y0,
                      D_800625A0->block358->value40[i * 2 + D_800625A0->bufferIndex].x1 -
                          D_800625A0->block358->value40[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->value40[i * 2 + D_800625A0->bufferIndex].y3 -
                          D_800625A0->block358->value40[i * 2 + D_800625A0->bufferIndex].y0);
    }
}

/* Lay out the detail panel's experience and experience to the next level
 * (seven digits each, by digit position) of party slot `slot`; `gear` shifts
 * them left. */
void func_801D74EC(u8 slot, u8 gear) {
    s32 i;
    u8 digit;

    func_801C80B8(D_8006D8A0[D_800625A0->party->ids[slot]].exp);
    D_800625A0->block358->expCount = 0;
    for (i = 0; i < 7; i++) {
        digit = D_800625A0->digits[2 + i];
        if (digit != 0xff) {
            D_800625A0->block358->expCount +=
                func_8002675C(D_800625A0->sheet, digit,
                              &D_800625A0->block358->exp[D_800625A0->block358->expCount * 2],
                              D_800625A0->bufferIndex, i * 8 + D_801E9D20 - gear * 0x18, D_801E9D24, 0x1000);
        }
    }
    for (i = 0; i < D_800625A0->block358->expCount; i++) {
        func_801C851C(D_800625A0->block358->expAt[i],
                      D_800625A0->block358->exp[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->exp[i * 2 + D_800625A0->bufferIndex].y0,
                      D_800625A0->block358->exp[i * 2 + D_800625A0->bufferIndex].x1 -
                          D_800625A0->block358->exp[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->exp[i * 2 + D_800625A0->bufferIndex].y3 -
                          D_800625A0->block358->exp[i * 2 + D_800625A0->bufferIndex].y0);
    }
    func_801C80B8(D_8006D8A0[D_800625A0->party->ids[slot]].expNext);
    D_800625A0->block358->expNextCount = 0;
    for (i = 0; i < 7; i++) {
        digit = D_800625A0->digits[2 + i];
        if (digit != 0xff) {
            D_800625A0->block358->expNextCount +=
                func_8002675C(D_800625A0->sheet, digit,
                              &D_800625A0->block358->expNext[D_800625A0->block358->expNextCount * 2],
                              D_800625A0->bufferIndex, i * 8 + D_801E9D28 - gear * 0x18, D_801E9D2C, 0x1000);
        }
    }
    for (i = 0; i < D_800625A0->block358->expNextCount; i++) {
        func_801C851C(D_800625A0->block358->expNextAt[i],
                      D_800625A0->block358->expNext[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->expNext[i * 2 + D_800625A0->bufferIndex].y0,
                      D_800625A0->block358->expNext[i * 2 + D_800625A0->bufferIndex].x1 -
                          D_800625A0->block358->expNext[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->expNext[i * 2 + D_800625A0->bufferIndex].y3 -
                          D_800625A0->block358->expNext[i * 2 + D_800625A0->bufferIndex].y0);
    }
}

/* Lay out the detail panel's value derived from record +77..+79 of party
 * slot `slot` ((+78 + +77) * 10 + +79) * 22 shown in hundredths with one
 * decimal) or, with `gear`, its gear's +68 (five digits). */
void func_801D7884(u8 slot, u8 gear) {
    s32 digits;
    s32 first;
    s32 x;
    s32 i;
    u8 digit;
    u16 value;
    u16 whole;
    s32 tenth;

    if (!gear) {
        value = ((D_8006D8A0[D_800625A0->party->ids[slot]].accessories[4] + D_8006D8A0[D_800625A0->party->ids[slot]].accessories[3]) *
                     10 +
                 D_8006D8A0[D_800625A0->party->ids[slot]].unk79) *
                22;
        whole = value / 100;
        digits = 3;
        first = 6;
        tenth = value;
        tenth = (tenth - whole * 100) / 10;
        func_801C80B8(whole);
        x = D_801E9D30;
    } else {
        digits = 5;
        func_801C80B8(D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unk68);
        first = 4;
        x = D_801E9D30 + 8;
    }
    D_800625A0->block358->list1C70Count = 0;
    for (i = 0; i < digits; i++) {
        digit = D_800625A0->digits[first + i];
        if (digit != 0xff) {
            D_800625A0->block358->list1C70Count +=
                func_8002675C(D_800625A0->sheet, digit,
                              &D_800625A0->block358->list1C70[D_800625A0->block358->list1C70Count * 2],
                              D_800625A0->bufferIndex, i * 8 + x - gear * 0x18, gear * 8 + D_801E9D34, 0x1000);
        }
    }
    if (!gear) {
        D_800625A0->block358->list1C70Count +=
            func_8002675C(D_800625A0->sheet, (u16)tenth,
                          &D_800625A0->block358->list1C70[D_800625A0->block358->list1C70Count * 2],
                          D_800625A0->bufferIndex, D_801E9D30 + 0x20, D_801E9D34, 0x1000);
    }
    for (i = 0; i < D_800625A0->block358->list1C70Count; i++) {
        func_801C851C(D_800625A0->block358->list1C70At[i],
                      D_800625A0->block358->list1C70[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->list1C70[i * 2 + D_800625A0->bufferIndex].y0,
                      D_800625A0->block358->list1C70[i * 2 + D_800625A0->bufferIndex].x1 -
                          D_800625A0->block358->list1C70[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block358->list1C70[i * 2 + D_800625A0->bufferIndex].y3 -
                          D_800625A0->block358->list1C70[i * 2 + D_800625A0->bufferIndex].y0);
    }
}

/* Build the status panels of party slot `slot` for `mode` and show them. */
void func_801D7C3C(u8 slot, u8 mode) {
    func_801D5ED4(slot, mode);
    func_801D6194(mode);
    func_801D6338(slot, mode);
    func_801D680C(slot, mode);
    func_801D6CF4(slot, mode);
    func_801D74EC(slot, mode);
    func_801D7884(slot, mode);
    func_801D7154(slot, mode);
    D_800625A0->party->redraw7 = 1;
    D_800625A0->block358->buffer = D_800625A0->bufferIndex;
}

/* Show the detail panel's two tabs (when `shown`), the one not selected by
 * `second` dimmed and semi-transparent. */
void func_801D7CFC(u8 slot, u8 shown, u8 second) {
    s32 i;
    s32 dim;

    second = second == 0; /* now: the tab to dim */
    D_800625A0->block358->tabCount = 0;
    if (shown) {
        for (i = 0; i < 2; i++) {
            func_8002675C(D_800625A0->sheet, D_801E977C[i], &D_800625A0->block358->tabs[i * 2],
                          D_800625A0->bufferIndex, i * 0x20 + 0x78, 0x5a, 0x1000);
            func_801C851C(D_800625A0->block358->tabsAt[i],
                          D_800625A0->block358->tabs[i * 2 + D_800625A0->bufferIndex].x0,
                          D_800625A0->block358->tabs[i * 2 + D_800625A0->bufferIndex].y0,
                          D_800625A0->block358->tabs[i * 2 + D_800625A0->bufferIndex].x1 -
                              D_800625A0->block358->tabs[i * 2 + D_800625A0->bufferIndex].x0,
                          D_800625A0->block358->tabs[i * 2 + D_800625A0->bufferIndex].y3 -
                              D_800625A0->block358->tabs[i * 2 + D_800625A0->bufferIndex].y0);
        }
        dim = second * 2;
        func_801E91C4(D_800625A0->block358->tabs + (dim + D_800625A0->bufferIndex));
        D_800625A0->block358->tabs[dim + D_800625A0->bufferIndex].tpage |= 0x20;
        (D_800625A0->block358->tabs + (dim + D_800625A0->bufferIndex))->r0 = 0x20;
        (D_800625A0->block358->tabs + (dim + D_800625A0->bufferIndex))->g0 = 0x20;
        (D_800625A0->block358->tabs + (dim + D_800625A0->bufferIndex))->b0 = 0x20;
        D_800625A0->block358->tabBuffer = D_800625A0->bufferIndex;
        D_800625A0->block358->tabCount = 2;
    }
}

/* Lay out the stat names of rows `first`..6 at (x, y) into the block at
 * +35c, dimming the odd rows, and place their quads. */
void func_801D7F50(s32 x, s32 y, u8 first) {
    s32 clutX;
    s32 clutY;
    s32 pageX;
    s32 pageY;
    s32 u;
    s32 v;
    s32 i;
    s32 start;
    s32 part;

    func_80026338(D_800625A0->sheet, 0xe0, &clutX, &clutY, &pageX, &pageY, &u, &v);
    D_800625A0->block35C->kind = 0;
    for (i = 0; i < 7 - first; i++) {
        start = D_800625A0->block35C->kind;
        D_800625A0->block35C->kind += func_8002675C(D_800625A0->sheet, D_801EA45C[first * 7 + i],
                                                    &D_800625A0->block35C->polys[start * 2], D_800625A0->bufferIndex,
                                                    x + D_801E9D40[i], y + D_801E9D5C[i], 0x1000);
        if (i & 1) {
            for (part = start; part < D_800625A0->block35C->kind; part++) {
                SetShadeTex(&D_800625A0->block35C->polys[part * 2 + D_800625A0->bufferIndex], 0);
                (D_800625A0->block35C->polys + (part * 2 + D_800625A0->bufferIndex))->r0 = 0x40;
                (D_800625A0->block35C->polys + (part * 2 + D_800625A0->bufferIndex))->g0 = 0x40;
                (D_800625A0->block35C->polys + (part * 2 + D_800625A0->bufferIndex))->b0 = 0x40;
            }
        }
    }
    for (i = 0; i < D_800625A0->block35C->kind; i++) {
        func_801C851C(&D_800625A0->block35C->verts[i * 4],
                      D_800625A0->block35C->polys[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block35C->polys[i * 2 + D_800625A0->bufferIndex].y0,
                      D_800625A0->block35C->polys[i * 2 + D_800625A0->bufferIndex].x1 -
                          D_800625A0->block35C->polys[i * 2 + D_800625A0->bufferIndex].x0,
                      D_800625A0->block35C->polys[i * 2 + D_800625A0->bufferIndex].y3 -
                          D_800625A0->block35C->polys[i * 2 + D_800625A0->bufferIndex].y0);
    }
}

/* Initialise the two gradient quads of a gauge in colour `colour` (0 pink,
 * 1 green, 2 red, 3 blue), fading to black. */
void func_801D827C(POLY_G4 *polys, u8 colour) {
    u8 rgb[3];
    s32 i;

    switch (colour) {
    case 0:
        rgb[0] = 0xff;
        rgb[1] = 0x80;
        rgb[2] = 0x80;
        break;
    case 1:
        rgb[0] = 0x80;
        rgb[1] = 0xff;
        rgb[2] = 0x80;
        break;
    case 2:
        rgb[0] = 0xff;
        rgb[1] = 0;
        rgb[2] = 0;
        break;
    case 3:
        rgb[0] = 0;
        rgb[1] = 0;
        rgb[2] = 0xff;
        break;
    }
    for (i = 0; i < 2; i++) {
        SetPolyG4(&polys[i]);
        polys[i].r0 = rgb[0];
        polys[i].g0 = rgb[1];
        polys[i].b0 = rgb[2];
        polys[i].r1 = rgb[0];
        polys[i].g1 = rgb[1];
        polys[i].b1 = rgb[2];
        polys[i].r2 = 0;
        polys[i].g2 = 0;
        polys[i].b2 = 0;
        polys[i].r3 = 0;
        polys[i].g3 = 0;
        polys[i].b3 = 0;
    }
}

/* Tint `count` quads of `polys` (every other one from `first`): 0 red,
 * 1 blue, 2 grey. */
void func_801D83AC(POLY_FT4 *polys, u8 colour, u8 count, u8 first) {
    u8 rgb[3];
    s32 i;
    POLY_FT4 *poly;

    rgb[1] = 0x40;
    switch (colour) {
    case 0:
        rgb[0] = 0x80;
        rgb[2] = 0x40;
        break;
    case 1:
        rgb[0] = 0x40;
        rgb[2] = 0x80;
        break;
    case 2:
        rgb[0] = 0x40;
        rgb[2] = 0x40;
        break;
    }
    for (i = 0; i < count; i++) {
        poly = &polys[first + i * 2];
        SetShadeTex(poly, 0);
        poly->r0 = rgb[0];
        poly->g0 = rgb[1];
        poly->b0 = rgb[2];
    }
}

/* Set up a gauge moving from `from` to `to` of `max`: its two lengths (of
 * 64) and the rising or falling look. */
void func_801D84B4(u16 from, u16 to, s32 max) {
    s32 diff;

    D_801EA6FC = from;
    D_801EA700 = to;
    diff = to - from;
    D_801EA704 = diff;
    D_801EA708 = from * 100 / max * 0x1900 / 10000;
    if (diff >= 0) {
        D_801EA710 = 2;
        D_801EA714 = 0xe3;
    } else {
        D_801EA710 = 3;
        D_801EA714 = 0xe5;
        D_801EA704 = from - to;
    }
    D_801EA70C = D_801EA704 * 100 / max * 0x1900 / 10000;
}

/* The largest of the seven values in each of `a` and `b`. */
s32 func_801D85DC(s32 unused, u16 *a, u16 *b) {
    u16 max;
    s32 i;
    u8 reserved[24]; /* the original frame reserves 24 unused bytes */

    max = 0;
    for (i = 0; i < 7; i++) {
        if (max < *a) {
            max = *a;
        }
        a++;
    }
    for (i = 0; i < 7; i++) {
        if (max < *b) {
            max = *b;
        }
        b++;
    }
    return max;
}

/* Draw the stat bars at (`x`, `y`) for rows `first`..6, scaled to the
 * largest stat (the unused first argument holds it): each row's bar,
 * value digits and their quads; with `compare` also the change bar against
 * the kept stats and, for a change, its signed digits tinted by the change's
 * colour. */
void func_801D8644(s32 scale, s32 x, s32 y, u8 compare, u8 first) {
    u16 *before;
    u16 *shown;
    s32 row;
    s32 i;
    s32 drawn;
    s32 start;

    if (!compare) {
        shown = D_800625A0->tables->shown;
        before = shown;
    } else {
        before = D_800625A0->labels360->stats;
        shown = D_800625A0->tables->shown;
    }
    scale = func_801D85DC(0, before, shown);
    for (row = 0; row < 7 - first; row++) {
        D_800625A0->block35C->rowACount[row] = 0;
        D_800625A0->block35C->rowBCount[row] = 0;
        func_801D84B4(before[row], shown[row], scale);
        func_801D827C(D_800625A0->block35C->bars[row], 0);
        func_801C851C(D_800625A0->block35C->barAt[row], D_801E9D78 + x, y + D_801E9D7C + row * 8, D_801EA708, 6);
        D_800625A0->block35C->barBuffer[row] = D_800625A0->bufferIndex;
        func_801C80B8(D_801EA6FC);
        for (i = 0; i < 4; i++) {
            if (D_800625A0->digits[5 + i] != 0xff) {
                D_800625A0->block35C->rowACount[row] +=
                    func_8002675C(D_800625A0->sheet, D_800625A0->digits[5 + i],
                                  &D_800625A0->block35C->rowA[row][D_800625A0->block35C->rowACount[row] * 2],
                                  D_800625A0->bufferIndex, x + D_801E9D80 + i * 8, y + D_801E9D84 + row * 8, 0x1000);
            }
        }
        for (i = 0; i < D_800625A0->block35C->rowACount[row]; i++) {
            func_801C851C(&D_800625A0->block35C->rowAAt[row][i * 4],
                          D_800625A0->block35C->rowA[row][i * 2 + D_800625A0->bufferIndex].x0,
                          D_800625A0->block35C->rowA[row][i * 2 + D_800625A0->bufferIndex].y0,
                          D_800625A0->block35C->rowA[row][i * 2 + D_800625A0->bufferIndex].x1 -
                              D_800625A0->block35C->rowA[row][i * 2 + D_800625A0->bufferIndex].x0,
                          D_800625A0->block35C->rowA[row][i * 2 + D_800625A0->bufferIndex].y3 -
                              D_800625A0->block35C->rowA[row][i * 2 + D_800625A0->bufferIndex].y0);
        }
        if (row & 1) {
            func_801D83AC(D_800625A0->block35C->rowA[row], 2, D_800625A0->block35C->rowACount[row],
                          D_800625A0->bufferIndex);
        }
        D_800625A0->block35C->rowABuffer[row] = D_800625A0->bufferIndex;
        if (compare) {
            func_801D827C(D_800625A0->block35C->highlights[row], D_801EA710);
            if (D_801EA710 == 2) {
                start = x + D_801E9D78 + D_801EA708;
            } else {
                start = x + D_801E9D78 + D_801EA708 - D_801EA70C;
            }
            func_801C851C(D_800625A0->block35C->highlightAt[row], start, y + D_801E9D7C + row * 8, D_801EA70C, 6);
            D_800625A0->block35C->highlightBuffer[row] = D_800625A0->bufferIndex;
            if (compare && D_801EA704 != 0) {
                D_800625A0->block35C->rowBCount[row] +=
                    func_8002675C(D_800625A0->sheet, D_801EA714, D_800625A0->block35C->rowB[row],
                                  D_800625A0->bufferIndex, x + D_801E9D80 + 0x20, y + D_801E9D84 + row * 8, 0x1000);
                func_801C80B8(D_801EA704);
                for (i = 0, drawn = 0; i < 3; i++) {
                    if (D_800625A0->digits[6 + i] != 0xff) {
                        D_800625A0->block35C->rowBCount[row] += func_8002675C(
                            D_800625A0->sheet, D_800625A0->digits[6 + i],
                            &D_800625A0->block35C->rowB[row][D_800625A0->block35C->rowBCount[row] * 2],
                            D_800625A0->bufferIndex, x + D_801E9D80 + 0x28 + drawn * 8, y + D_801E9D84 + row * 8, 0x1000);
                        drawn++;
                    }
                }
                for (i = 0; i < D_800625A0->block35C->rowBCount[row]; i++) {
                    func_801C851C(&D_800625A0->block35C->rowBAt[row][i * 4],
                                  D_800625A0->block35C->rowB[row][i * 2 + D_800625A0->bufferIndex].x0,
                                  D_800625A0->block35C->rowB[row][i * 2 + D_800625A0->bufferIndex].y0,
                                  D_800625A0->block35C->rowB[row][i * 2 + D_800625A0->bufferIndex].x1 -
                                      D_800625A0->block35C->rowB[row][i * 2 + D_800625A0->bufferIndex].x0,
                                  D_800625A0->block35C->rowB[row][i * 2 + D_800625A0->bufferIndex].y3 -
                                      D_800625A0->block35C->rowB[row][i * 2 + D_800625A0->bufferIndex].y0);
                }
                func_801D83AC(D_800625A0->block35C->rowB[row], D_801EA710 - 2, D_800625A0->block35C->rowBCount[row],
                              D_800625A0->bufferIndex);
                D_800625A0->block35C->rowBBuffer[row] = D_800625A0->bufferIndex;
                D_800625A0->block35C->highlighted = 1;
            }
        }
        D_800625A0->block35C->rowShown[row] = 1;
    }
}

/* Build the equipment panels of `slot` at the upper or (`lower`) lower place. */
void func_801D8DE4(u8 slot, u8 lower, u8 arg2, u8 mode) {
    s32 x;
    s32 y;

    x = 0x98;
    y = 0x26;
    if (lower) {
        x = 0x80;
        y = 0x90;
    }
    func_801D7F50(x, y, mode);
    func_801D8644(slot, x, y, arg2, mode);
    D_800625A0->party->unk8 = 1;
    D_800625A0->block35C->buffer = D_800625A0->bufferIndex;
}

/* Draw the part panel of party slot `slot` on the equipment screen: rows of
 * part names (weapon and accessories for `mode` 0, the special parts in rows
 * of four for 1 and 2), from the kept parts with `kept` and from the gear
 * with `gear`; a character's row 4 shows its portrait instead. */
void func_801D8EA4(u8 slot, u8 mode, u8 kept, u8 gear) {
    RECT rect;
    s32 rows;
    s32 column;
    s32 base;
    s32 i;
    u8 *weapons;
    u8 *specials;
    u8 *accessories;
    u8 *name;
    u8 *image;
    u8 load;

    rows = 5;
    column = 0xd0;
    base = 0;
    weapons = D_8006D8A0[D_800625A0->party->ids[slot]].weapons;
    specials = D_8006D8A0[D_800625A0->party->ids[slot]].specials;
    accessories = D_8006D8A0[D_800625A0->party->ids[slot]].accessories;
    if (mode) {
        rows = 4;
        column = 0x28;
        base = 9;
        if (mode == 1) {
            base = 5;
        }
        D_800625A0->labels360->visible[4] = 0;
    }
    if (gear) {
        weapons = D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unkC;
        specials = D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unk4;
        accessories = D_8006DFAC[D_8006D8A0[D_800625A0->party->ids[slot]].gear].unk9;
    }
    if (kept) {
        weapons = D_800625A0->labels360->parts[0];
        specials = D_800625A0->labels360->parts[1];
        accessories = D_800625A0->labels360->parts[2];
    }
    image = func_80031BDC(0x3f6, 0);
    for (i = 0; i < rows; i++) {
        switch (i) {
        case 0:
            if (mode != 2) {
                if (!gear) {
                    D_800625A0->labels360->labels[0].width = func_80034EAC(func_80033848(*weapons), image, 0x24, 0);
                } else {
                    D_800625A0->labels360->labels[0].width = func_80034EAC(func_80033A5C(*weapons), image, 0x24, 0);
                }
            } else if (!gear) {
                D_800625A0->labels360->labels[0].width = func_80034EAC(func_80033848(*specials), image, 0x24, 0);
            } else {
                D_800625A0->labels360->labels[0].width = func_80034EAC(func_80033A5C(*specials), image, 0x24, 0);
            }
            break;
        case 1:
        case 2:
        case 3:
        case 4:
            if (i != 4 || gear) {
                if (mode < 2) {
                    if (!gear) {
                        name = func_800337E8(accessories[i - 1]);
                    } else if (mode == 0) {
                        if (i == 1) {
                            D_800625A0->labels360->labels[1].width = func_80034EAC(func_80033A5C(weapons[3]), image, 0x24, 1);
                            goto shown;
                        }
                        name = func_80033A2C(accessories[i - 2]);
                    } else {
                        name = func_80033A2C(accessories[i - 1]);
                    }
                } else if (!gear) {
                    name = func_80033848(specials[i]);
                } else {
                    name = func_80033A5C(specials[i]);
                }
                D_800625A0->labels360->labels[i].width = func_80034EAC(name, image, 0x24, i % 2);
            }
            break;
        }
    shown:
        if (i & 1) {
            load = 1;
        } else if (!gear) {
            load = 0;
        } else if (mode == 0 && i == 4) {
            load = 1;
        }
        if (load) {
            rect.x = (i / 2 & 1) * 0x20 + 0x140;
            rect.y = i / 4 * 0xd + 0x27;
            rect.w = 0x28;
            rect.h = 0xd;
            LoadImage(&rect, image);
            DrawSync(0);
        }
        func_801E7C50(&D_800625A0->labels360->labels[i], i, 0xc, 0);
        if (i == 4 && !gear) {
            s32 page;

            page = GetTPage(0, 0, 0x180, 0);
            {
                s32 packetOffset = D_800625A0->bufferIndex * sizeof(POLY_FT4) +
                    4 * sizeof(MenuLabelSlot);
                ((POLY_FT4 *)((u8 *)D_800625A0->labels360 + packetOffset))->tpage = page;
            }
            {
                s32 packetOffset = D_800625A0->bufferIndex * sizeof(POLY_FT4) +
                    4 * sizeof(MenuLabelSlot);
                ((POLY_FT4 *)((u8 *)D_800625A0->labels360 + packetOffset))->clut =
                    ((D_8006D8A0[D_800625A0->party->ids[slot]].gear + 11) & 1) ? D_80059414 : D_800595D4;
            }
            (D_800625A0->labels360->labels[i].polys + D_800625A0->bufferIndex)->u0 = D_801EA578[slot + 3] * 4;
            (D_800625A0->labels360->labels[i].polys + D_800625A0->bufferIndex)->v0 = D_801EA5C4[slot + 3];
            (D_800625A0->labels360->labels[i].polys + D_800625A0->bufferIndex)->u1 = D_801EA578[slot + 3] * 4 + 0x60;
            (D_800625A0->labels360->labels[i].polys + D_800625A0->bufferIndex)->v1 = D_801EA5C4[slot + 3];
            (D_800625A0->labels360->labels[i].polys + D_800625A0->bufferIndex)->u2 = D_801EA578[slot + 3] * 4;
            (D_800625A0->labels360->labels[i].polys + D_800625A0->bufferIndex)->v2 = D_801EA5C4[slot + 3] + 0xd;
            (D_800625A0->labels360->labels[i].polys + D_800625A0->bufferIndex)->u3 = D_801EA578[slot + 3] * 4 + 0x60;
            (D_800625A0->labels360->labels[i].polys + D_800625A0->bufferIndex)->v3 = D_801EA5C4[slot + 3] + 0xd;
            D_800625A0->labels360->labels[i].width = 0x60;
        }
        func_801C851C(D_800625A0->labels360->labels[i].verts, column, D_801E9D88[i + base],
                      D_800625A0->labels360->labels[i].width, 0xd);
        D_800625A0->labels360->visible[i] = 1;
    }
    D_800625A0->labels360->count = D_800625A0->bufferIndex;
    D_800625A0->party->unk4B = 1;
    func_800320E8(image);
}

/* Step party slot `slot` forward (`dir` 0) or back (1) to the next occupied
 * slot, or with `readyOnly` to the next ready one; wraps around the three. */
s32 func_801D9704(s32 slot, u8 dir, u8 readyOnly) {
    switch (dir) {
    case 0:
        for (;;) {
            slot++;
            if (slot >= 3) {
                slot = 0;
            }
            if (!readyOnly) {
                if (D_800625A0->party->ids[slot] != 0xff) {
                    break;
                }
            } else if (D_800625A0->party->ready[slot] != 0) {
                break;
            }
        }
        break;
    case 1:
        for (;;) {
            slot--;
            if (slot < 0) {
                slot = 2;
            }
            if (!readyOnly) {
                if (D_800625A0->party->ids[slot] != 0xff) {
                    break;
                }
            } else if (D_800625A0->party->ready[slot] != 0) {
                break;
            }
        }
        break;
    }
    return slot;
}

/* The sound mode screen: choose one of the sound driver's output
 * modes (choice 0 is mode 0, 1 mode 2, 2 mode 1); confirm applies it, cancel
 * leaves. Always continues the menu. */
u8 func_801D9808(void) {
    s32 mode;
    u8 first;
    u8 stay;
    u8 apply;

    stay = 1;
    first = 1;
    apply = 0;
    do {
        func_801C7BF4();
        if (first) {
            func_801E8018(4, D_800625A0->soundLabels, D_801EA574, D_800625A0->party->unk5C);
            func_801E86C8(0);
            D_800625A0->choiceShown = 0xff;
            switch (func_80038824()) {
            case 0:
                mode = 0;
                break;
            case 1:
                mode = 2;
                break;
            case 2:
                mode = 1;
                break;
            }
            first = 0;
            D_800625A0->choice = mode;
        }
        if (D_800625A0->choice != D_800625A0->choiceShown) {
            func_801E8070(6, D_800625A0->soundLabels, (u8 *)D_801EA578, D_801E9F88, D_800625A0->party->unk5C,
                          D_800625A0->choice, 7, 0);
            func_801E8B4C(0);
            D_800625A0->choiceShown = D_800625A0->choice;
        }
        switch (D_800625A0->input) {
        case 4:
            apply = 1;
        case 5:
            stay = 0;
            break;
        case 1:
            if (D_800625A0->choice != 0) {
                D_800625A0->choice--;
            } else {
                D_800625A0->choice = D_800625A0->choiceCount - 1;
            }
            break;
        case 3:
            if (++D_800625A0->choice >= D_800625A0->choiceCount) {
                D_800625A0->choice = 0;
            }
            break;
        }
    } while (stay);
    if (apply) {
        switch (D_800625A0->choice) {
        case 0:
            mode = 0;
            break;
        case 1:
            mode = 2;
            break;
        case 2:
            mode = 1;
            break;
        }
        func_800386C4(mode);
    }
    func_801E8044(4, D_800625A0->party->unk5C);
    D_800625A0->party->redraw4 = 0;
    D_800625A0->party->redraw3 = 0;
    return 1;
}

/* Restart memory-card access: close the card events, reinitialise the card
 * library and open and enable its four events (ioe, error, timeout, new card). */
void func_801D9B08(void) {
    func_801C8960();
    VSync(0);
    InitCARD(1);
    StartCARD();
    _bu_init();
    DrawSync(0);
    VSync(0);
    EnterCriticalSection();
    D_800625A0->card->events[0] = OpenEvent(0xf4000001, 4, 0x2000, 0);
    D_800625A0->card->events[1] = OpenEvent(0xf4000001, 0x8000, 0x2000, 0);
    D_800625A0->card->events[2] = OpenEvent(0xf4000001, 0x100, 0x2000, 0);
    D_800625A0->card->events[3] = OpenEvent(0xf4000001, 0x2000, 0x2000, 0);
    EnableEvent(D_800625A0->card->events[0]);
    EnableEvent(D_800625A0->card->events[1]);
    EnableEvent(D_800625A0->card->events[2]);
    EnableEvent(D_800625A0->card->events[3]);
    ExitCriticalSection();
}

/* Enter the file screen's card mode: show message 20, wait for the view to
 * stop, restart card access with the CD callbacks saved and cleared, forget
 * the card presence and listings and check the cards now. Returns nonzero
 * when the check fails (the message stays up); otherwise closes the message. */
u8 func_801D9C84(void) {
    MenuCard *card;
    u8 failed;

    failed = 1;
    func_801D2F4C(0x20);
    D_800625A0->party->messageShown = 1;
    while (D_800625A0->viewMotion != 0) {
        func_801C7BF4();
    }
    D_800625A0->card->busy = 1;
    func_801C7BF4();
    func_801D9B08();
    DrawSync(0);
    VSync(0);
    EnterCriticalSection();
    D_801EA718 = CdSyncCallback(0);
    D_801EA71C = CdReadyCallback(0);
    D_801EA720 = CdReadCallback(0);
    ExitCriticalSection();
    card = D_800625A0->card;
    card->presentShown[0] = card->presentShown[1] = 0xff;
    D_800625A0->card->scanned[0] = 0;
    D_800625A0->card->scanned[1] = 0;
    D_800625A0->card->mode = 2;
    D_800625A0->cardPollTimer = 0x3c;
    func_801C7BF4();
    func_801C7BF4();
    if (func_801C93A8() == 0) {
        if (D_800625A0->party->messageShown != 0) {
            func_801D32B4();
            D_800625A0->party->messageShown = 0;
        }
        failed = 0;
    }
    return failed;
}

/* Leave the save/load screen: clear its images and listing, and close the
 * card events and handlers. */
void func_801D9E3C(void) {
    s32 i;

    D_800625A0->loadState = 0;
    func_801E64E0();
    func_801E5B3C();
    func_801E649C();
    for (i = 0; i < 32; i++) {
        D_800625A0->card->files[i].state = 0;
    }
    D_800625A0->card->unk4F8C[0] = 0xff;
    D_800625A0->card->unk4F8C[1] = 0xff;
    D_801EA900[1] = 0;
    D_801EA900[0] = 0;
    DrawSync(0);
    VSync(0);
    EnterCriticalSection();
    CdSyncCallback(D_801EA718);
    CdReadyCallback(D_801EA71C);
    CdReadCallback(D_801EA720);
    ExitCriticalSection();
}

/* Lay out the six file screen command labels (save or title variant). */
void func_801D9F34(void) {
    if (D_80059460 != 2) {
        func_801E8018(6, D_800625A0->fileLabels, D_801EA53C, D_800625A0->party->unk1A);
    } else {
        func_801E8018(6, D_800625A0->fileLabels, &D_801EA53C[6], D_800625A0->party->unk1A);
    }
}

/* The file screen (save or load by `save`): on the first pass build the card
 * panels, zoom in, show the header and enter card mode; then each frame
 * check the cards, show the choice labels and handle the input: confirm runs
 * the file command (and ends the screen outside the title), cancel ends it.
 * Returns 0 when a `loading` screen was left by the player, the cards failed
 * outside the field menu or there is no card outside the field menu. */
u8 func_801D9F98(u8 loading, u8 save) {
    u8 running;
    u8 first;
    u8 result;
    u8 header;

    running = 1;
    first = 1;
    result = 1;
    D_801E9784 = 0;
    D_800625A0->choice = 2;
    header = 0;
    if (D_80059460 == 0 && D_80059171 == 0) {
        D_800625A0->choice = 1;
    }
    D_800625A0->choiceShown = 0xff;
    func_801D9F34();
    func_801C7BF4();
    while (running) {
        D_800625A0->party->unkB = 0;
        D_800625A0->loadState = 1;
        if (first) {
            func_801E6450();
            func_801E5ACC();
            D_800625A0->card->present[0] = 1;
            D_800625A0->card->present[1] = 1;
            func_801D1E80();
            switch (D_80059460) {
            case 0:
                func_801D29A8(0, 0);
                if (D_80059171 == 0) {
                    header = 9;
                }
                break;
            case 2:
                header = 7;
                break;
            case 6:
                break;
            }
            func_801E86C8(header);
            D_800625A0->party->redraw6 = 0;
            first = 0;
            D_800625A0->party->unk20[1] = 0;
            if (func_801D9C84()) {
                if (D_80059460 != 0) {
                    result = 0;
                }
                break;
            }
            D_800625A0->party->cardMode = 1;
        }
        if (D_801E9778 != 0) {
            func_801D2F4C(0x20);
        }
        if (func_801C93A8()) {
            running = 0;
        }
        if (D_801E9778 != 0) {
            func_801D32B4();
            D_801E9778 = 0;
        }
        if (!running) {
            break;
        }
        if (D_800625A0->choice != D_800625A0->choiceShown) {
            func_801E8070(6, D_800625A0->fileLabels, &D_801EA53C[6], D_801E9EA0, D_800625A0->party->unk1A,
                          D_800625A0->choice, 7, 0);
            if (D_80059460 != 2) {
                func_801E8B4C(0);
            } else {
                func_801E8B4C(7);
            }
            D_800625A0->choiceShown = D_800625A0->choice;
        }
        switch (D_800625A0->input) {
        case 4:
            func_801D22C4();
            func_801E8044(6, D_800625A0->party->unk1A);
            running = func_801CD710(save);
            D_800625A0->choiceShown = 0xff;
            func_801D9F34();
            if (D_80059460 == 2) {
                break;
            }
        case 5:
            running = 0;
            if (loading) {
                result = 0;
            }
            break;
        case 1:
            if (D_800625A0->choice != 0) {
                D_800625A0->choice--;
            } else {
                D_800625A0->choice = D_800625A0->choiceCount - 1;
            }
            break;
        case 3:
            if (++D_800625A0->choice >= D_800625A0->choiceCount) {
                D_800625A0->choice = 0;
            }
            break;
        }
    }
    if (*(u16 *)D_800625A0->card->present == 0 && D_80059460 != 0) {
        result = 0;
    }
    D_800625A0->party->cardMode = 0;
    D_800625A0->party->unkB = 0;
    D_800625A0->party->redraw4 = 0;
    D_800625A0->party->redraw3 = 0;
    func_801E8044(6, D_800625A0->party->unk1A);
    func_801C8960();
    return result;
}

/* Open the save/load screen: its labels, its 1198-byte block and view 0. */
void func_801DA4A8(void) {
    void *block;

    func_801D22F4(2);
    func_801E8018(8, D_800625A0->labels10E0, D_801EA548, D_800625A0->party->unk38);
    block = func_80031BDC(0x1198, 0);
    D_800625A0->block42C = block;
    bzero(block, 0x1198);
    func_801C72BC(0);
}

/* Close the save/load screen: its labels, portraits 3-4, blocks and the
 * item table; view 10. */
void func_801DA518(void) {
    func_801D3444();
    func_801D4EA0(3);
    func_801D4EA0(4);
    D_800625A0->party->unk48 = 0;
    func_801C72BC(0x10);
    func_800320E8(D_800625A0->block42C->unk1180);
    func_800320E8(D_800625A0->block42C);
    func_800320E8(D_800625A0->tables->items);
}

/* Column of item entry `i`: its x offset and its name's left edge (8-aligned).
 * A statement macro (do/while (0)). */
#define ITEM_COLUMN(i, x, left)            \
    do {                                   \
        (x) = ((i) % 2) * 0x88;            \
        (left) = ((x) + 0x28) & 0xfff8;    \
    } while (0)

/* Build the 16 visible entries of the item list from scroll row `row`: an
 * entry without an id or count is emptied; otherwise the count is capped at
 * 99 and the item name and two-digit count are rendered into the image area
 * and laid out, greyed when the item cannot be used here. */
/* The counts are addressed from the ids (D_8006F65A - 150), as the original
 * does. */
void func_801DA5BC(s32 row) {
    u8 codes[4];
    u8 text[8];
    RECT rect;
    s32 i;
    u8 tens;
    s32 x;
    s32 left;
    u16 y;
    u8 kind;
    u8 grey;
    u8 *image;

    image = func_80031BDC(0x3f6, 0);
    codes[1] = 0;
    codes[3] = 0;
    for (i = 0; i < 16; i++) {
        if (D_8006F65A[row * 2 + i] != 0) {
            if ((D_8006F65A - 150)[row * 2 + i] != 0) {
                if ((D_8006F65A - 150)[row * 2 + i] >= 100) {
                    (D_8006F65A - 150)[row * 2 + i] = 99;
                }
                D_800625A0->block42C->names[i].width = func_80034EAC(func_80033818(D_8006F65A[row * 2 + i]), image, 0x24, 0);
                tens = (D_8006F65A - 150)[row * 2 + i] / 10;
                if (tens != 0) {
                    codes[0] = tens + 0x10;
                } else {
                    codes[0] = 0xc3;
                }
                codes[2] = (D_8006F65A - 150)[row * 2 + i] % 10 + 0x10;
                func_80033B34(codes, text, 2);
                D_800625A0->block42C->values[i].width = func_80034EAC(text, image, 0x24, 1);
                rect.x = (i & 1) * 0x18 + 0x180;
                rect.y = (i / 2) * 0xd + 0x80;
                rect.w = 0x28;
                rect.h = 0xd;
                LoadImage(&rect, image);
                DrawSync(0);
                kind = D_800625A0->tables->items[D_8006F65A[row * 2 + i]].use;
                if (kind & 0x20) {
                    grey = kind & 0x80;
                    if (D_80059171 == 0) {
                        grey = 0;
                    }
                } else {
                    grey = kind & 0x80;
                }
                func_801E7C50(&D_800625A0->block42C->names[i], i, 0x80, grey | 1);
                func_801E7C50(&D_800625A0->block42C->values[i], i, 0x80, grey | 2);
                ITEM_COLUMN(i, x, left);
                y = (i / 2) * 0x10 | 0xe;
                func_801C851C(D_800625A0->block42C->names[i].verts, left, y, D_800625A0->block42C->names[i].width,
                              0xd);
                left = (x + 0x90) & 0xfff8;
                func_801C851C(D_800625A0->block42C->values[i].verts, left, y, D_800625A0->block42C->values[i].width,
                              0xd);
                D_800625A0->block42C->names[i].count = D_800625A0->bufferIndex;
                D_800625A0->block42C->values[i].count = D_800625A0->bufferIndex;
                D_800625A0->block42C->shown[i] = 1;
            } else {
                D_8006F65A[row * 2 + i] = 0;
                D_800625A0->block42C->shown[i] = 0;
            }
        } else {
            (D_8006F65A - 150)[row * 2 + i] = 0;
            D_800625A0->block42C->shown[i] = 0;
        }
    }
    func_800320E8(image);
    D_800625A0->party->unk48 = 1;
}

/* Show the description of item list entry `entry` at scroll row `row`: the
 * item's message line, a copy of its name and count texts and, for items
 * used from the menu, its target labels. An empty entry hides it. */
void func_801DA9A8(s32 entry, s32 row) {
    RECT rect;
    u8 *ids;
    u8 *id;
    u8 *image;
    s32 pad;
    u8 all;
    u8 kind;
    u16 target;
    MenuItem *item;

    ids = D_8006F65A;
    id = &ids[row * 2 + entry];
    if (*id != 0) {
        image = func_80031BDC(0x618, 0);
        bzero(image, 0x618);
        D_800625A0->block42C->extra[2].width =
            func_80034EAC(func_80033728(D_800625A0->block42C->unk1180, *id), image, 0x39, 0);
        rect.x = 0x140;
        rect.y = 0x4e;
        rect.w = 0x3c;
        rect.h = 0xd;
        LoadImage(&rect, image);
        DrawSync(0);
        func_801E7C50(&D_800625A0->block42C->extra[2], 0, 0, 0);
        func_801E920C(&D_800625A0->block42C->extra[2].polys[D_800625A0->bufferIndex], 0x1c, 0xa1, 0, 0x4e,
                      D_800625A0->block42C->extra[2].width, 0xd);
        func_801C851C(D_800625A0->block42C->extra[2].verts, 0x1c, 0xa1, D_800625A0->block42C->extra[2].width, 0xd);
        func_800320E8(image);
        pad = D_800625A0->block42C->values[entry].width == 0x10 ? 4 : 0;
        memmove(&D_800625A0->block42C->extra[0], &D_800625A0->block42C->names[entry], sizeof(MenuLabelSlot));
        memmove(&D_800625A0->block42C->extra[1], &D_800625A0->block42C->values[entry], sizeof(MenuLabelSlot));
        func_801C851C(D_800625A0->block42C->extra[0].verts, 0x10, 0x93, D_800625A0->block42C->names[entry].width,
                      0xd);
        func_801C851C(D_800625A0->block42C->extra[1].verts, pad | 0x78, 0x93,
                      D_800625A0->block42C->values[entry].width, 0xd);
        (D_800625A0->block42C->extra[0].polys + D_800625A0->bufferIndex)->r0 = 0x80;
        (D_800625A0->block42C->extra[0].polys + D_800625A0->bufferIndex)->g0 = 0x80;
        (D_800625A0->block42C->extra[0].polys + D_800625A0->bufferIndex)->b0 = 0x80;
        SetSemiTrans(&D_800625A0->block42C->extra[0].polys[D_800625A0->bufferIndex], 0);
        (D_800625A0->block42C->extra[1].polys + D_800625A0->bufferIndex)->r0 = 0x80;
        (D_800625A0->block42C->extra[1].polys + D_800625A0->bufferIndex)->g0 = 0x80;
        (D_800625A0->block42C->extra[1].polys + D_800625A0->bufferIndex)->b0 = 0x80;
        SetSemiTrans(&D_800625A0->block42C->extra[1].polys[D_800625A0->bufferIndex], 0);
        func_801E8044(8, D_800625A0->party->unk38);
        item = &D_800625A0->tables->items[*id];
        if (item->use & 0xc0) {
            target = item->target;
            if (target & 0x4000) {
                all = 2;
            } else if (target & 0x1000) {
                all = 0;
            } else {
                all = 1;
            }
            func_801E8070(8, D_800625A0->labels10E0, D_801EA550, D_801E9EA0, D_800625A0->party->unk38, all, 0, 1);
            kind = (item->target & 3) + 3;
            func_801E8070(8, D_800625A0->labels10E0, D_801EA550, D_801E9EA0, D_800625A0->party->unk38, kind, 0, 1);
            (D_800625A0->labels10E0[all].polys + D_800625A0->bufferIndex)->r0 = 0x80;
            (D_800625A0->labels10E0[all].polys + D_800625A0->bufferIndex)->g0 = 0x80;
            (D_800625A0->labels10E0[all].polys + D_800625A0->bufferIndex)->b0 = 0x80;
            SetSemiTrans(&D_800625A0->labels10E0[all].polys[D_800625A0->bufferIndex], 0);
            (D_800625A0->labels10E0[kind].polys + D_800625A0->bufferIndex)->r0 = 0x80;
            (D_800625A0->labels10E0[kind].polys + D_800625A0->bufferIndex)->g0 = 0x80;
            (D_800625A0->labels10E0[kind].polys + D_800625A0->bufferIndex)->b0 = 0x80;
            SetSemiTrans(&D_800625A0->labels10E0[kind].polys[D_800625A0->bufferIndex], 0);
        }
        D_800625A0->block42C->extra[0].count = D_800625A0->bufferIndex;
        D_800625A0->block42C->extra[1].count = D_800625A0->bufferIndex;
        D_800625A0->block42C->extra[2].count = D_800625A0->bufferIndex;
        D_800625A0->block42C->extraShown = 1;
    } else {
        func_801E8044(8, D_800625A0->party->unk38);
        D_800625A0->block42C->extraShown = 0;
    }
}

/* Allocate image block `index` (+444). */
void func_801DB02C(u8 index) {
    void *block;

    block = func_80031BDC(0x78, 0);
    D_800625A0->blocks444[index] = block;
    bzero(block, 0x78);
    D_800625A0->blocks444[index]->frame = 4;
    D_800625A0->blocks444[index]->timer = 0;
}

/* Animate list cursor `index` (image block +444) and place it at entry
 * `entry` of the list kind `kind` (0 item list, 1 scrolled item list at
 * scroll row `row`, hidden when off the page, 2 two-column list, 3 one
 * column). */
void func_801DB0A8(s32 entry, s32 row, u8 kind, u8 index) {
    MenuImageBlock *cursor;
    POLY_FT4 *poly;
    s32 x;
    s32 y;
    s32 shown;

    cursor = D_800625A0->blocks444[index];
    shown = 1;
    if (++cursor->timer >= 6) {
        if (--cursor->frame < 0) {
            cursor->frame = 4;
        }
        cursor->timer = 0;
    }
    switch (kind) {
    case 0:
        x = (entry % 2) * 0x88 + 0x1c;
        y = (entry / 2) * 0x10 + 0x11;
        break;
    case 1:
        if (entry >= row * 2 && entry < row * 2 + 0x10) {
            x = (entry % 2) * 0x88 + 0x18;
            y = (entry - row * 2) / 2 * 0x10 + 0x11;
        } else {
            shown = 0;
        }
        break;
    case 2:
        x = (entry % 2) * 0x88 + 0x18;
        y = entry / 2 * 0x10 + 0x14;
        break;
    case 3:
        x = 0xa0;
        y = entry * 0xd + 0x14;
        shown = 2;
        break;
    }
    if (shown) {
        func_8002675C(D_800625A0->sheet, cursor->frame + 0x15b, cursor, D_800625A0->bufferIndex, 0, 0, 0x1000);
        poly = &cursor->polys[D_800625A0->bufferIndex];
        func_801C851C(cursor->verts, poly->x0 + x, poly->y0 + y, poly->x1 - poly->x0, poly->y3 - poly->y0);
        cursor->count = D_800625A0->bufferIndex;
        D_800625A0->party->unk50[index] = 1;
    } else {
        D_800625A0->party->unk50[index] = 0;
    }
}

/* Free image block `index` (+444) and clear its party flag (+50). */
void func_801DB340(u8 index) {
    func_800320E8(D_800625A0->blocks444[index]);
    D_800625A0->party->unk50[index] = 0;
}

/* Shade the item screen's texts by `mode` (801e8eac): windows 3 and 4, the
 * window quad, each built entry name and count, the two cursors, the
 * selected entry, the description and the eight help labels. */
void func_801DB39C(u8 mode) {
    s32 i;

    func_801E8F60(3, mode);
    func_801E8F60(4, mode);
    func_801E8EAC(&D_800625A0->block43C->polys[D_800625A0->block43C->buffer], mode);
    i = 0;
    do {
        if (D_800625A0->block42C->names[i].polys[D_800625A0->block42C->names[i].count].r0 != 0x20) {
            func_801E8EAC(&D_800625A0->block42C->names[i].polys[D_800625A0->block42C->names[i].count], mode);
            func_801E8EAC(&D_800625A0->block42C->values[i].polys[D_800625A0->block42C->values[i].count], mode);
        }
        i++;
    } while (i < 16);
    for (i = 0; i < 2; i++) {
        func_801E8EAC(&D_800625A0->blocks444[i]->polys[D_800625A0->blocks444[i]->count], mode);
    }
    func_801E8EAC(&D_800625A0->block42C->extra[0].polys[D_800625A0->block42C->extra[0].count], mode);
    func_801E8EAC(&D_800625A0->block42C->extra[1].polys[D_800625A0->block42C->extra[1].count], mode);
    func_801E8EAC(&D_800625A0->block42C->extra[2].polys[D_800625A0->block42C->extra[2].count], mode);
    for (i = 0; i < 8; i++) {
        func_801E8EAC(&D_800625A0->labels10E0[i].polys[D_800625A0->labels10E0[i].count], mode);
    }
}

/* Build the target selection's party panels: allocate the three panel
 * blocks once, clear them and build each occupied slot's panel (layout 1
 * for `mode` 2, where only characters with a gear qualify; `mode` then
 * becomes that layout flag); then place the three target cursor quads. */
void func_801DB5E4(u8 mode) {
    MenuAnchor *xs;
    MenuAnchor *ys;
    MenuPanel *panel;
    void *block;
    s32 i;
    u8 ok;
    u8 id;

    if (D_801E9785 == 0) {
        for (i = 0; i < 3; i++) {
            block = func_80031BDC(0xbec, 0);
            D_800625A0->panels[i] = block;
            bzero(block, 0xbec);
        }
        D_801E9785 = 1;
    }
    for (i = 0; i < 3; i++) {
        bzero(D_800625A0->panels[i], 0xbec);
    }
    if (mode != 2) {
        xs = D_801EA054;
        ys = D_801EA0DC;
        mode = 0;
    } else {
        xs = D_801EA098;
        ys = D_801EA120;
        mode = 1;
    }
    for (i = 0; i < 3; i++) {
        panel = D_800625A0->panels[i];
        id = D_800625A0->party->ids[i];
        ok = 1;
        if (id != 0xff) {
            if (mode) {
                ok = D_8006D8A0[id].gear != 0xff;
            }
            if (ok) {
                func_801CE0CC(panel, id, i, xs, ys, mode);
            }
        } else {
            panel->shown = 0;
        }
    }
    D_800625A0->party->unk46 = 1;
    for (i = 0; i < 3; i++) {
        (D_800625A0->markers->polys + (i * 2 + D_800625A0->markers->current[i]))->x0 = 0x90;
        (D_800625A0->markers->polys + (i * 2 + D_800625A0->markers->current[i]))->y0 = i * 0x38 + 0x30;
        (D_800625A0->markers->polys + (i * 2 + D_800625A0->markers->current[i]))->x1 = 0xa0;
        (D_800625A0->markers->polys + (i * 2 + D_800625A0->markers->current[i]))->y1 = i * 0x38 + 0x30;
        (D_800625A0->markers->polys + (i * 2 + D_800625A0->markers->current[i]))->x2 = 0x90;
        (D_800625A0->markers->polys + (i * 2 + D_800625A0->markers->current[i]))->y2 = i * 0x38 + 0x40;
        (D_800625A0->markers->polys + (i * 2 + D_800625A0->markers->current[i]))->x3 = 0xa0;
        (D_800625A0->markers->polys + (i * 2 + D_800625A0->markers->current[i]))->y3 = i * 0x38 + 0x40;
    }
}

/* Use the item at visible entry `entry` of scroll row `row`: for an item
 * usable here, select targets (the whole party for all-target items, else
 * the cursor's slot; left/right move it) and use the item on them on
 * confirm until it runs out or the player cancels. Returns the targets
 * marked last (0 when cancelled or unusable). The item ids are read through
 * `ids`, taken from the inventory record at entry and again for the clear. */
u8 func_801DB920(s32 row, s32 entry) {
    u16 marks;
    u8 running;
    u8 redraw;
    s32 slot;
    u8 used;
    s32 all;
    s32 i;
    MenuItem *item;
    u8 *ids;

    marks = 0;
    running = 1;
    slot = D_800625A0->firstMember;
    D_801E9785 = 0;
    ids = INVENTORY->ids;
    item = &D_800625A0->tables->items[ids[row * 2 + entry]];
    redraw = 1;
    if (item->use & 0x80) {
        marks = 1;
        if (item->use & 0x20) {
            marks = D_80059171 != 0;
        }
    }
    all = item->target & 1;
    if (marks) {
        func_801D397C(2, 0x10, 0xe, 0x90, 0xb0, 0, 0, 4, 0);
        while (running) {
            func_801C7BF4();
            if (redraw) {
                redraw = 0;
                func_801DA5BC(row);
                func_801DA9A8(entry, row);
                func_801DB39C(1);
                func_801DB5E4(0);
            }
            marks = 0;
            D_800625A0->markers->visible[0] = D_800625A0->markers->visible[1] = D_800625A0->markers->visible[2] = 0;
            if (all) {
                for (i = 0; i < 3; i++) {
                    if (D_800625A0->party->ids[i] != 0xff) {
                        marks |= 1 << i;
                        D_800625A0->markers->visible[i] = 1;
                    }
                }
            } else {
                marks = 1 << slot;
                D_800625A0->markers->visible[slot] = 1;
            }
            D_800625A0->party->unk2F = 1;
            if (INVENTORY->counts[row * 2 + entry] == 0) {
                running = 0;
            }
            if (!running) {
                break;
            }
            switch (D_800625A0->input) {
            case 4:
                used = 0;
                for (i = 0; i < 3; i++) {
                    if (func_801C865C(marks, i) &&
                        func_801E31C0(D_800625A0->tables, D_800625A0->party->ids[i], D_8006F65A[row * 2 + entry]) == 0) {
                        used |= 1;
                    }
                }
                if (used) {
                    func_801C8574(0x37);
                    redraw = 1;
                    if (--INVENTORY->counts[row * 2 + entry] == 0) {
                        ids = INVENTORY->ids;
                        ids[row * 2 + entry] = 0;
                    }
                } else {
                    func_801C8574(4);
                    redraw = 1;
                }
                break;
            case 5:
                running = 0;
                marks = 0;
                break;
            case 1:
                slot = func_801D9704(slot, 0, 0);
                break;
            case 3:
                slot = func_801D9704(slot, 1, 0);
                break;
            }
        }
        D_800625A0->markers->visible[0] = D_800625A0->markers->visible[1] = D_800625A0->markers->visible[2] = 0;
        func_801DB39C(0);
        D_800625A0->party->unk46 = 0;
        func_801C7BF4();
        for (i = 0; i < 3; i++) {
            func_800320E8(D_800625A0->panels[i]);
        }
        D_801E9785 = 0;
        func_801D4EA0(2);
    }
    return marks;
}

/* Swap inventory entries `a` and `b` (item id and count). */
void func_801DBD4C(s32 a, s32 b) {
    u8 tmp;

    tmp = D_8006F65A[a];
    D_8006F65A[a] = D_8006F65A[b];
    D_8006F65A[b] = tmp;
    tmp = D_8006F5C4[a];
    D_8006F5C4[a] = D_8006F5C4[b];
    D_8006F5C4[b] = tmp;
}
