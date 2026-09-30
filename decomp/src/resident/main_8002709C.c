#include "common.h"
#include "psyq/libapi.h"
#include "psyq/libc.h"
#include "psyq/libcd.h"
#include "psyq/libetc.h"
#include "psyq/libgpu.h"
#include "psyq/libgte.h"
#include "psyq/libsn.h"
#include "psyq/libspu.h"
#include "mode.h"
#include "menu.h"
#include "sprite.h"
#include "cd.h"
#include "stream.h"
#include "model.h"
#include "heap.h"
#include "text.h"
#include "pad.h"
#include "console.h"
#include "sound.h"

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002709C", func_8002709C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002709C", func_800273C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002709C", func_800278F8);

/* Release a block if there is one. */
void func_80027D40(void *block) {
    if (block != NULL) {
        func_800320E8(block);
    }
}

/* Set up a texture scroll of `count` bands over an area; the band phases are allocated (heap tag 4) and cleared. Returns the scroll, or NULL. */
TextureScroll *func_80027D64(TextureScroll *scroll, s16 x, s16 y, s16 w, s16 h, s16 count, u16 source_x,
                             u16 source_y, s8 *speeds) {
    s32 i;

    func_80032498(4, 0);
    scroll->x = x;
    scroll->y = y;
    scroll->w = w;
    scroll->h = h;
    scroll->step = h / count;
    scroll->count = count;
    scroll->source_x = source_x;
    scroll->source_y = source_y;
    scroll->speeds = speeds;
    scroll->phases = func_80031BDC(count * 2, 0);
    if (scroll->phases == NULL) {
        scroll = NULL;
    } else {
        for (i = 0; i < count; i++) {
            scroll->phases[i] = 0;
        }
    }
    return scroll;
}

/* Advance each band's phase by its speed and redraw the area rotated by it (two MoveImage copies per band). */
void func_80027EAC(TextureScroll *scroll) {
    RECT rect;
    s32 i;
    u16 line;
    u16 offset;
    u16 rest;

    if (scroll->phases == NULL) {
        return;
    }
    rect.y = scroll->y;
    rect.h = scroll->step;
    line = scroll->source_y;
    for (i = 0; i < scroll->count; i++) {
        scroll->phases[i] += scroll->speeds[i];
        offset = (u16)((s16)scroll->phases[i] >> 4) % scroll->w;
        rest = scroll->w - offset;
        rect.x = scroll->x;
        rect.w = offset;
        MoveImage(&rect, scroll->source_x + rest, line);
        rect.x = offset + scroll->x;
        rect.w = rest;
        MoveImage(&rect, scroll->source_x, line);
        line += scroll->step;
        rect.y += scroll->step;
    }
}

/* Release a texture scroll's phases. */
void func_8002800C(TextureScroll *scroll) {
    if (scroll->phases != NULL) {
        func_800320E8(scroll->phases);
        scroll->phases = NULL;
    }
}

/* Disc error indicator: draw a coloured bar for retry `level`; from the fourth, switch to a text screen showing the failing file forever. */
void func_8002804C(s32 level, s32 r, s32 g, s32 b) {
    RECT rect;
    struct {
        DRAWENV draw;
        DISPENV disp;
        u_long ot[16];
    } screen;

    rect.x = level * 10 + 10;
    rect.w = 8;
    rect.y = 0;
    rect.h = 0x1C0;
    if (level + 1 >= 4) {
        r = 0xFF;
        g = 0xFF;
        b = 0xFF;
    }
    ClearImage(&rect, r, g, b);
    DrawSync(0);
    if (level + 1 >= 4) {
        ResetGraph(0);
        InitGeom();
        SetDefDrawEnv(&screen.draw, 0, 0, 0x140, 0x100);
        SetDefDispEnv(&screen.disp, 0, 0, 0x140, 0xF0);
        screen.draw.dtd = 0;
        screen.draw.isbg = 0;
        screen.draw.dfe = 1;
        screen.disp.isinter = 0;
        PutDrawEnv(&screen.draw);
        PutDispEnv(&screen.disp);
        func_800374E8(0x10, 0x10, 0x280, 0xF0, 0x400, 0, 0x280, 0, 0x280, 0x100, 0);
        SetDispMask(1);
        rect.x = 0;
        rect.y = 0;
        rect.w = 0x280;
        rect.h = 0x30;
        ClearImage(&rect, 0, 0, 0);
        for (;;) {
            ClearOTagR(screen.ot, 8);
            func_8003700C("\n%d", D_8004FE14 + D_80059F0C - 1);
            func_8003700C("\n%s", func_80028998(D_80059F0C));
            func_80037324(screen.ot);
            DrawOTag(&screen.ot[7]);
            DrawSync(0);
        }
    }
}

/* Initialise disc access: the CD library (or the PC file server for other modes), the file index and directory table, reading both from sectors 24 and 40 when booting from CD. */
void func_80028230(u8 *files, u16 *directories, u32 mode) {
    D_8005A488 = 0;
    D_8005A48C = 0;
    D_8005A490 = 0;
    D_8005A494 = 0;
    D_8005A498 = 0;
    D_8005A49C = 0;
    D_8005A4A4 = 0;
    D_8005A4A8 = 0;
    D_8005A4B4 = 0;
    if (mode == 0 || mode == -1) {
        while (CdInit() == 0) {
        }
        CdSetDebug(0);
        CdDataCallback(0);
        CdSyncCallback(0);
        CdReadyCallback(0);
        CdControl(7, 0, D_80059F1C);
        func_8002A428(0xA0);
        func_80028A60(0);
        VSync(3);
    } else {
        func_8004C38C();
    }
    if (mode != -1) {
        D_8004FE48 = (char *)mode;
    } else {
        D_8004FE48 = NULL;
    }
    D_8004FDF0 = files;
    D_8004FDF4 = directories;
    D_8004FE14 = 0;
    D_8004FDFC = 0;
    D_8004FDF8 = 0;
    D_8004FE1C = 0;
    D_8004FE4C = -1;
    if (mode == 0) {
        func_8002954C(0x18, files, 0x8000, 0, 0);
        func_80028A60(0);
        func_8002954C(0x28, D_8004FDF4, 0x7A, 0, 0);
        func_80028A60(0);
    }
}

/* End disc access: stop the read, pause the drive (on CD) and clear the CD callbacks. */
void func_800283D4(void) {
    func_8002A498(0);
    func_80028A60(0);
    if (D_8004FE48 == NULL) {
        while (CdControlB(9, 0, D_80059F1C) == 0) {
        }
        func_8002A428(0xA0);
        func_80028A60(0);
        VSync(3);
    }
    CdDataCallback(0);
    CdSyncCallback(0);
    CdReadyCallback(0);
    D_8004FDFC = 0;
    D_8004FDF8 = 0;
    D_8004FE1C = 0;
}

/* Select a directory by group and index in the directory table; returns it, or -1 (selecting 0) when that entry is empty. */
s32 func_80028470(s32 group, s32 index) {
    D_8004FE14 = D_8004FDF4[group + index] - 1;
    if (D_8004FE14 < 0) {
        D_8004FE14 = 0;
        return -1;
    }
    return D_8004FE14;
}

/* The directory group (a multiple of four) and index of the selected directory, both zero when none matches; returns the selection. */
s32 func_800284B4(s32 *group, s32 *index) {
    u16 *entry = D_8004FDF4;
    s32 i;

    for (i = 0; i < 0x40; i++, entry++) {
        if (*entry == D_8004FE14 + 1) {
            *group = i / 4 * 4;
            *index = i - i / 4 * 4;
            break;
        }
    }
    if (i == 0x40) {
        *group = 0;
        *index = 0;
    }
    return D_8004FE14;
}

/* The disc number (directory table word 0x3c). */
s32 func_80028530(void) {
    return D_8004FDF4[0x3C];
}

/* The directory at group + index relative to the selected one. */
s32 func_80028548(s32 group, s32 index) {
    return D_8004FDF4[group + index] - D_8004FE14;
}

/* Load a whole PC file into a new heap block (four tries per file-server call); returns the block, or NULL. */
void *func_80028570(char *name, s32 *size) {
    s32 fd;
    s32 length;
    s32 read;
    s32 i;
    void *block;

    for (i = 0; i < 4; i++) {
        fd = PCopen(name, 0, 0);
        if (fd != -1) {
            break;
        }
    }
    if (fd != -1) {
        length = PClseek(fd, 0, 2);
        if (size != NULL) {
            *size = length;
        }
        PClseek(fd, 0, 0);
        block = func_80031BDC(length, 0);
        read = 0;
        if (block != NULL) {
            for (i = 0; i < 4; i++) {
                read = func_8004C398(fd, block, length);
                if (read != 0) {
                    break;
                }
            }
        }
        if (read == 0) {
            if (block != NULL) {
                func_800320E8(block);
            }
            block = NULL;
        }
        for (i = 0; i < 4; i++) {
            if (PCclose(fd) == 0) {
                goto done;
            }
        }
        if (block != NULL) {
            func_800320E8(block);
        }
    }
    block = NULL;
done:
    return block;
}

s32 func_800286BC(void) {
    return D_8004FDF8;
}

/* Nonzero while a disc read is pending, the drive is busy or a read is in progress. */
s32 func_800286CC(void) {
    s32 state = D_8004FDFC;

    if (state == 0) {
        if (D_8004FE48 == NULL && CdDataSync(1) != 0) {
            return 1;
        }
        if (D_8004FE1C != 0) {
            return 1;
        }
    }
    return state;
}

/* A file's byte size: from the PC file server when it knows the file, else from the file index. */
s32 func_80028738(s32 file) {
    s32 fd;
    s32 size;
    u8 *entry;

    if (D_8004FE48 != NULL) {
        fd = PCopen(func_80028998(file), 0, 0);
        size = PClseek(fd, 0, 2);
        PCclose(fd);
        if (size > 0) {
            goto done;
        }
    }
    entry = &D_8004FDF0[(file + D_8004FE14 - 1) * 7];
    size = (entry[6] << 24) + (entry[5] << 16) + (entry[4] << 8) + entry[3];
done:
    return size;
}

/* As 80028738 in the second directory selection (8004fe18), rounded up to words. */
s32 func_80028808(s32 file) {
    s32 fd;
    s32 size;
    u8 *entry;

    if (D_8004FE48 != NULL) {
        fd = PCopen(func_80028998(file), 0, 0);
        size = PClseek(fd, 0, 2);
        PCclose(fd);
        if (size > 0) {
            return (size + 3) / 4 * 4;
        }
    }
    entry = &D_8004FDF0[(file + D_8004FE18 - 1) * 7];
    size = (entry[6] << 24) + (entry[5] << 16) + (entry[4] << 8) + entry[3];
    return (size + 3) / 4 * 4;
}

/* A file's byte size rounded up to words. */
s32 func_800288EC(s32 file) {
    s32 size = func_80028738(file);

    return (size + 3) / 4 * 4;
}

/* For an index entry with a negative size (a directory), its file count; 0 for a file. */
s16 func_80028928(s32 file) {
    u8 *entry = &D_8004FDF0[(file + D_8004FE14 - 1) * 7];
    s32 size = (entry[6] << 24) + (entry[5] << 16) + (entry[4] << 8) + entry[3];

    if (size < 0) {
        return -size;
    }
    return 0;
}

/* The PC file server name of a file (64 bytes per file), or NULL without the server. */
char *func_80028998(s32 file) {
    char *name = NULL;

    if (D_8004FE48 != NULL) {
        name = D_8004FE48 + (file + D_8004FE14 - 1) * 64;
    }
    return name;
}

/* A file's first sector in the selected directory. */
s32 func_800289D0(s32 file) {
    u8 *entry = &D_8004FDF0[(file + D_8004FE14 - 1) * 7];

    return ((entry[2] << 16) + (entry[1] << 8)) | entry[0];
}

/* A file's first sector in the second directory selection (8004fe18). */
s32 func_80028A18(s32 file) {
    u8 *entry = &D_8004FDF0[(file + D_8004FE18 - 1) * 7];

    return ((entry[2] << 16) + (entry[1] << 8)) | entry[0];
}

/* Wait for the disc (mode 0: until idle); returns the disc status. */
s32 func_80028A60(s32 mode) {
    if (mode == 0) {
        while (func_800286CC() > 0) {
        }
    }
    return func_800286CC();
}

/* Replace the shared ring and return the previous one. */
StreamRing *func_80028A94(StreamRing *ring) {
    StreamRing *previous = D_8004FE30;
    D_8004FE30 = ring;
    return previous;
}

/* Clear every slot; the first slot's third halfword (ring offset 8) keeps
 * the low count. Returns the count, or -1 without a ring. */
s32 func_80028AAC(void) {
    StreamRing *ring = D_8004FE30;
    StreamSlot *slots;
    s32 i;
    s32 count;

    if (ring == NULL) {
        return -1;
    }
    count = ring->count;
    slots = ring->slots;
    for (i = 0; i < count; i++) {
        slots[i].state = 0;
        slots[i].sequence = 0;
        slots[i].length = 0;
        slots[i].w6 = 0;
    }
    slots->length = count;
    return count;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002709C", func_80028B14);

/* Whether any of `count` ring slots from `index` differs from `state` or runs past the last slot. */
s32 func_80028E60(s32 index, s32 count, s32 state) {
    s32 i;

    for (i = 0; i < count; i++) {
        if (D_8004FE2C[index].state != state) {
            return 1;
        }
        if (++index > D_8004FE40) {
            return 1;
        }
    }
    return 0;
}

/* Merge the free run that follows slot `index` into its length. */
void func_80028ECC(s32 index) {
    s16 length = D_8004FE2C[index].length;
    s32 next = index + length;

    if (next < D_8004FE40 && D_8004FE2C[next].state == 0) {
        D_8004FE2C[index].length = length + D_8004FE2C[next].length;
    }
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002709C", func_80028F30);

/* Release a ring chunk: clear its slot's state and return the old state (0xffff without a ring, 0 for no chunk). */
/* Nonmatching: the original keeps the ring in $a1 (so the 0xffff return fills the branch delay slot); GCC puts it in $v0. */
#ifdef NON_MATCHING
u16 func_8002945C(u8 *chunk) {
    StreamRing *ring = D_8004FE30;
    StreamSlot *slots;
    s32 index;
    u16 state;
    u8 *payload;

    if (ring == NULL) {
        return 0xFFFF;
    }
    slots = ring->slots;
    if (chunk == NULL) {
        return 0;
    }
    payload = (u8 *)ring + ring->count * 8 + 0x24;
    index = (u32)(chunk - payload) >> 11;
    state = slots[index].state;
    slots[index].state = 0;
    return state;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002709C", func_8002945C);
#endif

/* Release a run of ring chunks (the chunk header's halfword 3 counts them), merge the freed run and return the first slot's old state. */
u16 func_800294B4(u8 *chunk) {
    StreamRing *ring = D_8004FE30;
    StreamSlot *slots;
    s32 index;
    s32 i;
    u16 state;
    u8 *payload;

    if (ring == NULL) {
        return 0xFFFF;
    }
    slots = ring->slots;
    if (chunk == NULL) {
        return 0;
    }
    i = ((u16 *)chunk)[3];
    payload = (u8 *)ring + ring->count * 8 + 0x24;
    index = (u32)(chunk - payload) >> 11;
    state = slots[index].state;
    for (; i > 0; i--) {
        slots[index + i - 1].state = 0;
    }
    func_80028ECC(index);
    return state;
}

/* Read `size` bytes from a raw disc sector (CD only); -1 with the PC file server. */
s32 func_8002954C(s32 sector, void *destination, s32 size, s32 a3, s32 a4) {
    if (D_8004FE48 != NULL) {
        return -1;
    }
    func_80028A60(0);
    D_8004FE04 = sector;
    D_8004FDF8 = size;
    return func_80029690(0, destination, a3, a4);
}

/* Read a file of the selected directory; -3 for an invalid file, an empty file or no destination. */
s32 func_800295D8(s32 file, void *destination, s32 a2, s32 a3) {
    if (file <= 0 || func_80028738(file) <= 0 || destination == NULL) {
        return -3;
    }
    func_80028A60(0);
    D_8004FE18 = D_8004FE14;
    D_8004FE04 = func_800289D0(file);
    D_8004FDF8 = func_800288EC(file);
    return func_80029690(file, destination, a2, a3);
}

/* Start reading `D_8004FDF8` bytes from sector D_8004FE04: into a stream ring (flags 0x100, or 0x200 with CD mode byte flags | 0xa0) or into memory; with the PC file server the file is opened (ring) or read now. */
/* GCC 2.6.3 disc-unit code; under 2.6.3 + --expand-div this C differs only in the file/destination register assignment (s3/s2 swapped). */
#ifdef NON_MATCHING
s32 func_80029690(s32 file, void *destination, s32 mode, s32 flags) {
    StreamRing *ring;
    char *name;
    s32 fd;
    s32 i;
    CdlLOC *position = &D_80059F10;
    u8 *mode_byte;

    D_80059F0C = file;
    for (i = 2; i >= 0; i--) {
        D_80059EF8[i] = 0;
    }
    D_8004FDFC = 1;
    D_8004FE08 = destination;
    D_8004FE38 = mode & 0xFFFF;
    D_8004FE10 = 0;
    D_8004FE0C = NULL;
    D_8004FE34 = 0;
    D_8005A4DC = 0;
    CdIntToPos(D_8004FE04, position);
    if (flags & 0x100) {
        func_80028A94(destination);
        ring = D_8004FE30;
        if (ring->count == 0) {
            return -4;
        }
        D_8004FE08 = (u8 *)ring + ring->count * 8 + 0x24;
        D_8004FE2C = ring->slots;
        D_8004FE40 = ring->count;
        D_8004FE26 = 0;
        D_8004FE28 = 0;
        D_8004FE24 = 0;
        func_80028AAC();
        if (D_8004FE48 != NULL) {
            name = func_80028998(file);
            for (i = 0; i < 4; i++) {
                D_8004FE4C = PCopen(name, 0, 0);
                if (D_8004FE4C != -1) {
                    break;
                }
                func_8002804C(i, 0xFF, 0, 0);
            }
            return D_8004FE4C == -1 ? -3 : 0;
        }
        D_8004FE1C = 1;
        CdDataCallback(func_8002BA58);
        CdSyncCallback(func_8002A68C);
        CdReadyCallback(func_8002B2F0);
    } else if (flags & 0x200) {
        func_80028A94(destination);
        ring = D_8004FE30;
        if (ring->count == 0) {
            return -4;
        }
        D_8004FE08 = (u8 *)ring + ring->count * 8 + 0x24;
        D_8004FE2C = ring->slots;
        D_8004FE40 = ring->count;
        D_80059F60 = 0;
        D_8004FE26 = 0;
        D_8004FE28 = 0;
        D_8004FE24 = 0;
        func_80028AAC();
        for (i = 3, mode_byte = &D_80059F18[3]; i >= 0; i--) {
            *mode_byte-- = 0;
        }
        D_80059F18[0] = flags | 0xA0;
        if (D_8004FE48 == NULL) {
            return 0;
        }
        name = func_80028998(file);
        for (i = 0; i < 4; i++) {
            D_8004FE4C = PCopen(name, 0, 0);
            if (D_8004FE4C != -1) {
                break;
            }
            func_8002804C(i, 0xFF, 0, 0);
        }
        return D_8004FE4C == -1 ? -3 : 0;
    } else {
        if (D_8004FE48 != NULL) {
            name = func_80028998(file);
            for (i = 0; i < 4; i++) {
                fd = PCopen(name, 0, 0);
                if (fd != -1) {
                    goto opened;
                }
                func_8002804C(i, 0xFF, 0, 0);
            }
            if (fd == -1) {
                return -4;
            }
        opened:
            if (destination != NULL) {
                for (i = 0; i < 4; i++) {
                    if (func_8004C398(fd, destination, D_8004FDF8) != 0) {
                        break;
                    }
                    func_8002804C(i, 0, 0xFF, 0);
                }
            }
            for (i = 0; i < 4; i++) {
                if (PCclose(fd) == 0) {
                    D_8004FDF8 = 0;
                    D_8004FDFC = 0;
                    return 0;
                }
                func_8002804C(i, 0, 0, 0xFF);
            }
            return -6;
        }
        D_8004FE1C = 1;
        CdDataCallback(NULL);
        CdSyncCallback(func_8002A68C);
        CdReadyCallback(func_8002B084);
    }
    D_8005A488++;
    CdControlF(2, (u8 *)position);
    return 0;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002709C", func_80029690);
#endif

/* Read a zero-terminated file list: sort it by file, then start the CD reads (the callbacks continue them), or with the PC file server read every file now. Returns 0, or -3 for an empty list. */
s32 func_80029AFC(FileRequest *list, s32 mode, s32 unused) {
    s32 count;
    s32 i;
    s32 j;
    s32 best;
    u16 file;
    void *destination;
    char *name;
    s32 fd;

    if (list == NULL) {
        return -3;
    }
    for (count = 0; list[count].file != 0; count++) {
    }
    if (count == 0) {
        return -3;
    }
    for (i = 0; i < count - 1; i++) {
        file = list[i].file;
        best = i;
        for (j = i + 1; j < count; j++) {
            if (list[j].file < file) {
                best = j;
                file = list[j].file;
            }
        }
        file = list[i].file;
        destination = list[i].destination;
        list[i].file = list[best].file;
        list[i].destination = list[best].destination;
        list[best].file = file;
        list[best].destination = destination;
    }
    func_80028A60(0);
    D_8004FE18 = D_8004FE14;
    for (i = 2; i >= 0; i--) {
        D_80059EF8[i] = 0;
    }
    D_8004FE10 = 0;
    D_8004FE0C = list;
    D_8004FDFC = count;
    D_8004FE00 = count;
    D_8004FE08 = list->destination;
    file = list->file;
    if (file == 0 || D_8004FE08 == NULL) {
        func_8002A394(mode);
        D_8004FDF8 = 0;
        D_8004FDFC = 0;
        return 0;
    }
    D_80059F0C = file;
    D_8004FE04 = func_800289D0(file);
    D_8004FDF8 = func_80028808(file);
    D_8004FE38 = mode & 0xFFFF;
    D_8004FE3C = 0;
    D_8004FE34 = 0;
    D_8005A4DC = 0;
    CdIntToPos(D_8004FE04, &D_80059F10);
    if (D_8004FE48 != NULL) {
        for (i = 0; i < count; i++) {
            file = list[i].file;
            D_80059F0C = file;
            name = func_80028998(file);
            for (j = 0; j < 4; j++) {
                fd = PCopen(name, 0, 0);
                if (fd != -1) {
                    goto opened;
                }
                func_8002804C(j, 0xFF, 0, 0);
            }
            goto close;
        opened:
            if (list[i].destination != NULL) {
                for (j = 0; j < 4; j++) {
                    if (func_8004C398(fd, list[i].destination, func_80028808(file)) != 0) {
                        break;
                    }
                    func_8002804C(j, 0, 0xFF, 0);
                }
            }
        close:
            for (j = 0; j < 4; j++) {
                if (PCclose(fd) == 0) {
                    break;
                }
                func_8002804C(j, 0, 0, 0xFF);
            }
        }
        D_8004FDF8 = 0;
        D_8004FDFC = 0;
        return 0;
    }
    D_8004FE1C = 1;
    CdDataCallback(func_8002BA40);
    CdSyncCallback(func_8002A68C);
    CdReadyCallback(func_8002AC24);
    D_8005A488++;
    CdControlF(2, (u8 *)&D_80059F10);
    return 0;
}

/* Start streaming a file through a ring of at least two slots with six stream parameters; with the PC file server the whole stream is pumped now. Returns 0, -3 for a bad file, -4 for a bad ring, -6 when the file does not close. */
s32 func_80029EB0(s32 file, StreamRing *ring, s32 mode, s32 unused, u16 a, u16 b, u16 c, u16 d, u16 e, u16 f) {
    s32 count;
    char *name;
    s16 i;

    if (ring == NULL || (u32)(count = ring->count) < 2) {
        return -4;
    }
    if (file <= 0) {
        return -3;
    }
    if (func_80028738(file) <= 0) {
        return -3;
    }
    func_80028A60(0);
    D_8004FE18 = D_8004FE14;
    for (i = 0; i < 3; i++) {
        D_80059EF8[i] = 0;
    }
    func_80028A94(ring);
    D_80059F0C = file;
    D_8004FE04 = func_800289D0(file);
    D_8004FDF8 = func_800288EC(file);
    D_8004FDFC = 1;
    D_8004FE08 = (u8 *)ring + count * 8 + 0x24;
    D_8004FE2C = ring->slots;
    D_8004FE38 = mode & 0xFFFF;
    D_8004FE10 = 0;
    D_8004FE40 = count;
    D_8004FE26 = 0;
    D_8004FE28 = 0;
    D_8004FE0C = NULL;
    D_8004FE34 = 0;
    D_8005A4DC = 0;
    D_80059F24 = a;
    D_80059F28 = b;
    D_80059F2C = c;
    D_80059F30 = d;
    D_80059F34 = e;
    D_80059F38 = f;
    D_80059F3C = 0;
    D_80059F40 = 0;
    D_80059F44 = 0;
    D_80059F48 = 0;
    D_80059F4C = 0;
    D_80059F50 = 0;
    func_80028AAC();
    CdIntToPos(D_8004FE04, &D_80059F10);
    if (D_8004FE48 != NULL) {
        name = func_80028998(file);
        for (i = 0; i < 4; i++) {
            D_80059F04 = PCopen(name, 0, 0);
            if (D_80059F04 != -1) {
                break;
            }
            func_8002804C(i, 0xFF, 0, 0);
        }
        do {
            func_8002B8B0(0, 0);
            func_8002BF38(0, 0);
        } while (D_8004FDFC > 0);
        for (i = 0; i < 4; i++) {
            i = PCclose(D_80059F04);
            if (i == 0) {
                break;
            }
            func_8002804C(i, 0, 0, 0xFF);
        }
        if (i != 0) {
            return -6;
        }
        D_8004FDFC = 0;
        D_8004FDF8 = 0;
        return 0;
    }
    D_8004FE1C = 1;
    CdDataCallback(func_8002BB50);
    CdSyncCallback(func_8002A68C);
    CdReadyCallback(func_8002B5D0);
    D_8005A488++;
    CdControlF(2, (u8 *)&D_80059F10);
    return 0;
}
