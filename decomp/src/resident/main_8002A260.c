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

/* Allocate a stream ring of `count` 2,048-byte sectors (plus the slot
 * header), then select and reset it. Returns the ring or NULL. */
StreamRing *func_8002A260(s32 count, s32 mode) {
    StreamRing *ring;

    if (count > 0) {
        ring = func_80031BDC(count * 0x808 + 0x24, mode);
        if (ring == NULL) {
            return NULL;
        }
        ring->count = count;
        func_80028A94(ring);
        func_80028AAC();
        return ring;
    }
    return NULL;
}

/* Unless a read is already running, seek to `file` (or pause for a
 * nonpositive file) with the resident CD ready callback installed. */
void func_8002A2D0(s32 file) {
    if (D_8004FE48 == 0 && func_800286CC() == 0) {
        D_8004FE18 = D_8004FE14;
        if (file > 0) {
            CdIntToPos(func_800289D0(file), &D_80059F10);
            D_8004FE1C = 3;
            CdSyncCallback(func_8002A68C);
            CdControlF(2, (u8 *)&D_80059F10);
        } else {
            D_8004FE1C = 5;
            CdSyncCallback(func_8002A68C);
            CdControlF(9, NULL);
        }
    }
}

/* Seek to `file` (or pause) unconditionally. */
void func_8002A394(s32 file) {
    if (file > 0) {
        CdIntToPos(func_800289D0(file), &D_80059F10);
        D_8004FE1C = 3;
        CdSyncCallback(func_8002A68C);
        CdControlF(2, (u8 *)&D_80059F10);
    } else {
        D_8004FE1C = 5;
        CdSyncCallback(func_8002A68C);
        CdControlF(9, NULL);
    }
}

/* Issue CdlSetmode with `mode`. */
void func_8002A428(u8 mode) {
    u8 *param;
    s32 i;

    D_8004FE1C = 9;
    CdSyncCallback(func_8002A68C);
    for (i = 3, param = &D_80059F18[3]; i >= 0; i--) {
        *param-- = 0;
    }
    D_80059F18[0] = mode;
    CdControlF(0xE, D_80059F18);
}

/* Request a stop with `reason`; when a read is active, drop it and close the
 * open host file handle (retrying a few transient results). */
void func_8002A498(s32 reason) {
    s32 result;

    D_8004FE34 = 1;
    D_8004FE38 = reason;
    if (D_8004FE48 != 0) {
        D_8004FDF8 = 0;
        D_8004FDFC = 0;
        if (D_8004FE4C != -1) {
            do {
                result = PCclose(D_8004FE4C);
            } while (result != 0 && result + 1 < 4);
            D_8004FE4C = -1;
        }
    }
}

/* Free every loaded file's data in a zero-terminated file table. */
void func_8002A524(FileEntry *table) {
    FileEntry *entry;
    void *data;

    if (table->id != 0) {
        entry = table;
        do {
            data = entry->data;
            entry++;
            if (data != NULL) {
                func_800320E8(data);
            }
        } while (entry->id != 0);
    }
}

/* Load the files following `first` into `table` (allocated when NULL). On an
 * allocation failure everything loaded is freed and NULL returned.
 * Nonmatching: GCC strength-reduces &table[i]; the original recomputes it. */
#ifdef NON_MATCHING
FileEntry *func_8002A57C(s32 first, FileEntry *table) {
    s32 count;
    s32 owned = 0;
    s32 i;

    count = func_80028928(first);
    if (count > 0) {
        if (table == NULL) {
            table = func_80031BDC((count + 1) * 8, 0);
            owned = 1;
            if (table == NULL) {
                return NULL;
            }
        }
        for (i = 0; i < count; i++) {
            table[i].id = first + i + 1;
            table[i].data = func_80031BDC(func_800288EC(first + i + 1), 0);
            if (table[i].data == NULL) {
                func_8002A524(table);
                if (owned > 0) {
                    func_800320E8(table);
                }
                return NULL;
            }
        }
        table[count].id = 0;
        table[count].data = NULL;
    } else {
        table = NULL;
    }
    return table;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002A57C);
#endif

/* CD command-complete callback: advance the seek/read state machine
 * (D_8004FE1C). Status 2 is success; on failure the retry reason is kept in
 * D_8004FE20 and the drive status is polled (state 10) until it can retry. */
void func_8002A68C(u8 status, u8 *result) {
    switch (D_8004FE1C) {
    case 0:
        break;
    case 1:
        if (status == 2) {
            D_8005A48C++;
            D_8004FE1C++;
            CdControlF(6, NULL);
        } else {
            D_8005A490++;
            D_80059F08 = CdReadyCallback(NULL);
            D_8004FE20 = 3;
            D_8004FE1C = 10;
            CdControlF(1, NULL);
        }
        break;
    case 2:
        if (status == 2) {
            D_8005A48C++;
            CdSyncCallback(NULL);
            D_8004FE1C = 0;
        } else {
            D_8005A490++;
            D_80059F08 = CdReadyCallback(NULL);
            D_8004FE20 = 3;
            D_8004FE1C = 10;
            CdControlF(1, NULL);
        }
        break;
    case 3:
        if (status == 2) {
            D_8004FE1C++;
            CdControlF(0x15, NULL);
        } else {
            D_8004FE20 = 1;
            D_8004FE1C = 10;
            CdControlF(1, NULL);
        }
        break;
    case 4:
        if (status == 2) {
            CdSyncCallback(NULL);
            D_8004FE1C = 0;
        } else {
            D_8004FE20 = 1;
            D_8004FE1C = 10;
            CdControlF(1, NULL);
        }
        break;
    case 5:
        if (status == 2) {
            CdSyncCallback(NULL);
            D_8004FE1C = 0;
        } else {
            D_8004FE20 = 2;
            D_8004FE1C = 10;
            CdControlF(1, NULL);
        }
        break;
    case 6:
        if (status == 2) {
            D_8004FE1C = 1;
            D_8005A488++;
            D_8005A494++;
            CdReadyCallback(D_80059F08);
            CdControlF(2, (u8 *)&D_80059F10);
        } else {
            D_8004FE20 = 3;
            D_8004FE1C = 10;
            D_8005A498++;
            CdControlF(1, NULL);
        }
        break;
    case 7:
        if (status == 2) {
            D_8004FE1C = 6;
            D_8005A4A8++;
            CdControlF(9, NULL);
        } else {
            D_8004FE20 = 4;
            D_8004FE1C = 10;
            D_8005A4B4++;
            CdControlF(1, NULL);
        }
        break;
    case 12:
        if (status == 2) {
            D_8004FE1C = 8;
            D_80059F14.file = 1;
            D_80059F14.chan = *(u8 *)&D_8004FE38; /* the mode's channel byte */
            CdControlF(0xD, (u8 *)&D_80059F14);
        } else {
            D_8004FE20 = 5;
            D_8004FE1C = 10;
            CdControlF(1, NULL);
        }
        break;
    case 8:
        if (status == 2) {
            D_8004FE1C = 1;
            D_8005A488++;
            D_8005A494++;
            CdReadyCallback(D_80059F08);
            CdControlF(2, (u8 *)&D_80059F10);
        } else {
            D_8004FE20 = 5;
            D_8004FE1C = 10;
            D_8005A498++;
            CdControlF(1, NULL);
        }
        break;
    case 9:
        if (status == 2) {
            D_8004FE1C = 5;
            CdControlF(9, NULL);
        } else {
            D_8004FE20 = 6;
            D_8004FE1C = 10;
            D_8005A4B4++;
            CdControlF(1, NULL);
        }
        break;
    case 10:
        if (status == 2 && !(result[0] & 0x10)) {
            D_8004FE1C = 11;
            CdControlF(0x13, NULL);
        } else {
            D_8004FE1C = 10;
            CdControlF(1, NULL);
        }
        break;
    case 11:
        if (status == 2) {
            switch (D_8004FE20) {
            case 1:
                D_8004FE1C = 3;
                CdControlF(2, (u8 *)&D_80059F10);
                break;
            case 2:
                D_8004FE1C = 5;
                CdControlF(9, NULL);
                break;
            case 3:
                D_8004FE1C = 6;
                CdControlF(9, NULL);
                break;
            case 4:
                D_8004FE1C = 7;
                CdControlF(8, NULL);
                break;
            case 5:
                D_8004FE1C = 12;
                CdControlF(0xE, D_80059F18);
                break;
            case 6:
                D_8004FE1C = 9;
                CdControlF(0xE, D_80059F18);
                break;
            }
        } else {
            D_8004FE1C = 10;
            CdControlF(1, NULL);
        }
        break;
    }
}

/* CD data callback of list reads: copy each sector of the current file of
 * the list (D_8004FE0C) to its destination, move on to the next file (reading
 * on through a short gap, or seeking), and retry through the command state
 * machine when a sector fails or arrives out of order.
 * Nonmatching: the original loads 0x200 into $a1 before the rounding branch
 * of the tail sector count (and divides in $v0); this schedules it after. */
#ifdef NON_MATCHING
void func_8002AC24(u8 status, u8 *result) {
    FileRequest *request;
    u16 file;
    u32 first;
    s32 i;
    s32 j;

    if (status == 1) {
        if (D_8004FE34 > 0) {
            CdReadyCallback(NULL);
            CdDataCallback(NULL);
            D_8004FDF8 = 0;
            func_8002A394(D_8004FE38);
            D_8004FE00 = 0;
            D_8004FDFC = 0;
            return;
        }
        if (D_8004FDF8 >= 0x800) {
            if (D_8004FE3C == 0) {
                CdGetSector(D_80059EF8, 3);
                CdGetSector(D_8004FE08, 0x200);
            }
        } else if (D_8004FDF8 > 0 && D_8004FE3C == 0) {
            CdGetSector(D_80059EF8, 3);
            CdGetSector(D_8004FE08, (D_8004FDF8 + 3) / 4);
            CdGetSector(D_800596F8, 0x200 - (D_8004FDF8 + 3) / 4);
        }
        if (CdPosToInt((CdlLOC *)D_80059EF8) != D_8004FE04 && D_8004FE3C == 0) {
            D_8004FDE8++;
            goto failed;
        }
        D_8004FE08 = (u8 *)D_8004FE08 + 0x800;
        D_8004FDF8 -= 0x800;
        D_8004FE04++;
        if (D_8004FDF8 > 0) {
            return;
        }
        D_8004FE10++;
        file = D_8004FE0C[D_8004FE10].file;
        D_8004FE08 = D_8004FE0C[D_8004FE10].destination;
        if (file != 0 && D_8004FE08 != NULL) {
            first = func_80028A18(file);
            D_8004FDF8 = func_80028808(file);
            if (D_8004FE04 < first && D_8004FE04 + D_8004FDE0 >= first) {
                /* Close ahead: read on, discarding the sectors between. */
                D_8004FE3C = 1;
                D_8004FDF8 = (D_8004FE04 - first) << 11;
                D_8004FE10--;
                return;
            }
            if (first == D_8004FE04) {
                D_8004FE3C = 0;
                D_8004FE00--;
                return;
            } else {
                D_8004FE3C = 0;
                D_8004FE04 = first;
                D_80059F08 = CdReadyCallback(NULL);
                CdIntToPos(D_8004FE04, &D_80059F10);
                D_8004FE1C = 6;
                CdSyncCallback(func_8002A68C);
                CdControlF(9, NULL);
            }
            D_8004FE00--;
            return;
        }
        D_8004FDF8 = 0;
        CdReadyCallback(NULL);
        func_8002A394(D_8004FE38);
        D_8004FE00 = 0;
        D_8004FDFC = 0;
        return;
    }
failed:
    D_8005A4DC++;
    D_80059F08 = CdReadyCallback(NULL);
    CdIntToPos(D_8004FE04, &D_80059F10);
    if (D_8005A4DC < 3) {
        D_8004FE20 = 3;
    } else {
        for (i = 9999; i >= 0; i--) {
            for (j = 1999; j >= 0; j--) {
            }
        }
        D_8005A4DC = 0;
        D_8004FE20 = 4;
        D_8005A4A4++;
    }
    D_8004FE1C = 10;
    CdSyncCallback(func_8002A68C);
    CdControlF(1, NULL);
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002AC24);
#endif

/* CD data callback of single-file reads: copy each sector to the
 * destination (the tail of a short last sector to D_800596F8), finish the
 * read after the last one, and retry through the command state machine when
 * a sector fails or arrives out of order.
 * Nonmatching: as in 8002AC24, the original loads the 0x200 of the tail
 * count into the rounding branch's delay slot. */
#ifdef NON_MATCHING
void func_8002B084(u8 status, u8 *result) {
    s32 i;
    s32 j;

    if (status == 1) {
        if (D_8004FE34 <= 0) {
            if (D_8004FDF8 >= 0x800) {
                CdGetSector(D_80059EF8, 3);
                CdGetSector(D_8004FE08, 0x200);
            } else if (D_8004FDF8 > 0) {
                CdGetSector(D_80059EF8, 3);
                CdGetSector(D_8004FE08, (D_8004FDF8 + 3) / 4);
                CdGetSector(D_800596F8, 0x200 - (D_8004FDF8 + 3) / 4);
            }
            if (CdPosToInt((CdlLOC *)D_80059EF8) != D_8004FE04) {
                D_8004FDE4++;
                goto failed;
            }
            D_8004FE04++;
            D_8004FE08 = (u8 *)D_8004FE08 + 0x800;
            D_8004FDF8 -= 0x800;
            if (D_8004FDF8 > 0) {
                return;
            }
        }
        CdReadyCallback(NULL);
        D_8004FDF8 = 0;
        func_8002A394(D_8004FE38);
        D_8004FDFC = 0;
        return;
    }
failed:
    D_8005A4DC++;
    D_80059F08 = CdReadyCallback(NULL);
    CdIntToPos(D_8004FE04, &D_80059F10);
    if (D_8005A4DC < 3) {
        D_8004FE20 = 3;
    } else {
        for (i = 9999; i >= 0; i--) {
            for (j = 1999; j >= 0; j--) {
            }
        }
        D_8005A4DC = 0;
        D_8004FE20 = 4;
        D_8005A4A4++;
    }
    D_8004FE1C = 10;
    CdSyncCallback(func_8002A68C);
    CdControlF(1, NULL);
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002B084);
#endif

/* CD data callback of stream reads: store each sector in the next free slot
 * of the stream ring (numbering it with D_8004FE26), stop after the last one,
 * and retry through the command state machine when a sector arrives out of
 * order or no slot is free.
 * Nonmatching: the original reloads D_8004FE26 for the increment and orders
 * the slot and destination additions the other way round. */
#ifdef NON_MATCHING
void func_8002B2F0(u8 status, u8 *result) {
    StreamSlot *slot;
    s32 index;
    s32 tried;
    s32 i;
    s32 j;

    if (status == 1) {
        if (D_8004FE34 > 0) {
            CdReadyCallback(NULL);
            CdDataCallback(NULL);
            D_8004FDF8 = 0;
            func_8002A394(D_8004FE38);
            D_8004FDFC = 0;
            return;
        }
        if (D_8004FDF8 > 0) {
            for (tried = 0; tried < D_8004FE40; tried++) {
                index = D_8004FE10++;
                slot = &D_8004FE2C[index];
                if (D_8004FE10 >= D_8004FE40) {
                    D_8004FE10 = 0;
                }
                if (slot->state == 0) {
                    break;
                }
            }
            if (slot->state != 0) {
                goto retry;
            }
            CdGetSector(D_80059EF8, 3);
            if (CdPosToInt((CdlLOC *)D_80059EF8) != D_8004FE04) {
                D_8004FDEC++;
                CdGetSector(D_800596F8, 0x200);
                goto failed;
            }
            slot->state = 1;
            slot->sequence = D_8004FE26++;
            CdGetSector((u8 *)D_8004FE08 + index * 0x800, 0x200);
            D_8004FDF8 -= 0x800;
            D_8004FE04++;
            if (D_8004FDF8 > 0) {
                return;
            }
        }
        CdReadyCallback(NULL);
        D_8004FDF8 = 0;
        return;
    }
failed:
    D_8005A4DC++;
retry:
    D_80059F08 = CdReadyCallback(NULL);
    CdIntToPos(D_8004FE04, &D_80059F10);
    if (D_8005A4DC < 3) {
        D_8004FE20 = 3;
    } else {
        for (i = 9999; i >= 0; i--) {
            for (j = 1999; j >= 0; j--) {
            }
        }
        D_8005A4DC = 0;
        D_8004FE20 = 4;
        D_8005A4A4++;
    }
    D_8004FE1C = 10;
    CdSyncCallback(func_8002A68C);
    CdControlF(1, NULL);
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002B2F0);
#endif

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002B5D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002B8B0);


void func_8002BA40(void) {
    D_8004FDFC = D_8004FE00;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002BA58);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002BB50);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002BF38);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002C310);

/* Nonzero when files come from the PC file server (its name table). */
s32 func_8002C3D8(void) {
    return (s32)D_8004FE48;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002C3E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002C4BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002C59C);

/* Trim a model group's heap block to its data (once). Returns 1 when it
 * was already trimmed. */
s32 func_8002C644(ModelGroup *group) {
    if (group->flags & 2) {
        return 1;
    }
    group->flags |= 2;
    func_80031F70((u8 *)group, group->primitives - (u8 *)group);
    return 0;
}

/* Trim a model buffer's heap block at its end (once). Returns 1 when it
 * was already trimmed. */
s32 func_8002C68C(ModelBuffer *buffer) {
    if (buffer->flags & 0x40) {
        return 1;
    }
    buffer->flags |= 0x40;
    func_80031F70((u8 *)buffer, buffer->end - (u8 *)buffer);
    buffer->end = NULL;
    return 0;
}

extern u8 D_80059598;
extern u8 D_80059599;
extern u8 D_8005959A;

void func_8002C6E0(u8 r, u8 g, u8 b) {
    D_80059598 = r;
    D_80059599 = g;
    D_8005959A = b;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002C700);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002C8CC);

/* Allocate a model buffer's two halves of `size` bytes each. */
void func_8002CB54(ModelBuffer *buffer, u8 **first, u8 **second) {
    u8 *block;

    func_800324B8(0x25);
    block = func_80031BDC(buffer->size * 2, 0);
    *first = block;
    *second = block + buffer->size;
}

/* Release a model buffer's owned block. */
void func_8002CBBC(ModelBuffer *buffer) {
    if (buffer->flags & 1) {
        func_800320E8(buffer->buffer);
        buffer->flags &= ~1;
    }
}

extern s32 D_80050108; /* texture page override: 0 none, 1 page, 2 raw */
extern s32 D_8005010C; /* CLUT override: 0 on */
extern s32 D_80059310;
extern s32 D_80059314;

/* Override model texture pages with the page at (x, y). */
void func_8002CC10(u16 x, u16 y) {
    D_80059310 = GetTPage(0, 0, x, y) & 0x1F;
    D_80050108 = 1;
}

void func_8002CC54(u16 tpage) {
    D_80059310 = tpage;
    D_80050108 = 2;
}

/* Override model CLUTs with the CLUT at (x, y). */
void func_8002CC74(u16 x, u16 y) {
    D_80059314 = GetClut(x, y) & 0xFFF0;
    D_8005010C = 0;
}

void func_8002CCAC(void) {
    D_80050108 = 0;
    D_8005010C = 1;
}

extern u16 D_80059308;
extern u16 D_8005930C;

/* Apply the texture page override to a primitive's page. */
void func_8002CCC8(u16 *tpage) {
    u16 value = *tpage;

    D_80059308 = value;
    if (D_80050108 == 1) {
        D_80059308 = value & 0xFFE0;
        D_80059308 = (value & 0xFFE0) | D_80059310;
    } else if (D_80050108 == 2) {
        D_80059308 = D_80059310;
    }
}

/* Apply the CLUT override to a primitive's CLUT. */
void func_8002CD24(u16 *clut) {
    u16 value = *clut;

    D_8005930C = value;
    if (D_8005010C == 0) {
        D_8005930C = value & 0xF;
        D_8005930C = (value & 0xF) | D_80059314;
    }
}

/* Handle a texture page (0xC4) or CLUT (0xC8) command. Returns 1 for any
 * other command. */
s32 func_8002CD64(u8 *command) {
    if ((command[3] & 0xF0) != 0xC0) {
        return 1;
    }
    switch (command[3]) {
    case 0xC4:
        func_8002CCC8((u16 *)command);
        return 0;
    case 0xC8:
        func_8002CD24((u16 *)command);
        return 0;
    }
    return 1;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002CDCC);

s32 func_8002CF34(s32 *value) {
    RenderPacket *packet = D_80059424;

    packet->code = 4;
    packet->value = *value;
    return 1;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002CF58);

s32 func_8002D0C0(s32 *value) {
    RenderPacket *packet = D_80059424;

    packet->code = 5;
    packet->value = *value;
    return 1;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002D0E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002D180);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002D244);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002D354);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002D420);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002D530);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002D6AC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002D77C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002D814);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002D984);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002DA14);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002DAFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002DB84);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002DC9C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002DD20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002DDE4);

/* The shared unpack buffer. */
u8 *func_8002DFE0(void) {
    return D_8006FAF0;
}

extern s32 D_800500F8;
extern s32 D_800500FC;

void func_8002DFF0(s32 a, s32 b) {
    D_800500FC = (b - 1) << 16;
    D_800500F8 = a;
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002E010);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002E448);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002E64C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002E8B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002EAB8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002ED20);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002EEF8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002F0E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002F2E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002F4B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002F6B4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002F8D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002FAE8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002FCFC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002FF0C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8003014C);

/* Copy the vertices listed in `indices` (last first) from `in` to `out`. */
void func_800301C8(SVECTOR *out, SVECTOR *in, s32 count, s16 *indices) {
    s32 i;
    s32 k;

    for (i = count - 1; i != -1; i--) {
        k = indices[i];
        out[k].vx = in[k].vx;
        out[k].vy = in[k].vy;
        out[k].vz = in[k].vz;
    }
}

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_80030228);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_800302D4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_800303C8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_800305D8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_800306D0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_80030750);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_80030988);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_80030A30);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_80030B14);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_80030C40);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_80030C78);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_80030C98);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_80030EE8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8003101C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_800315A0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_800315C4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_800315E8);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8003160C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_80031630);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_80031654);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_80031678);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8003169C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_800316C0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_800316E4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_80031708);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8003172C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_80031750);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_80031774);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_80031798);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_800317BC);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_800317E0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_80031804);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_80031828);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8003184C);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_80031870);
