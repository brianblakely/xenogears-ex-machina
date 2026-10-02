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
 * machine when a sector fails or arrives out of order. */
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

/* CD data callback of single-file reads: copy each sector to the
 * destination (the tail of a short last sector to D_800596F8), finish the
 * read after the last one, and retry through the command state machine when
 * a sector fails or arrives out of order. */
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

/* CD data callback of stream reads: store each sector in the next free slot
 * of the stream ring (numbering it with D_8004FE26), stop after the last one,
 * and retry through the command state machine when a sector arrives out of
 * order or no slot is free.
 * Nonmatching: the original reloads D_8004FE26 for the increment, copies the
 * slot index before incrementing it and stores the slot state first. */
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

/* A second, identical copy of the stream data callback 8002B2F0.
 * Nonmatching as 8002B2F0. */
#ifdef NON_MATCHING
void func_8002B5D0(u8 status, u8 *result) {
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
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002B5D0);
#endif

/* Stream data step of PC file server reads, called in place of the CD
 * data callback: read a sector from the file server into the next free ring slot
 * (up to four read attempts); stop the read when no slot is free or after
 * the last sector.
 * Nonmatching: the slot index copy and the slot state store (see 8002B2F0). */
#ifdef NON_MATCHING
void func_8002B8B0(void) {
    StreamSlot *slot;
    s32 index;
    s16 i;

    if (D_8004FDF8 > 0) {
        for (i = 0; i < D_8004FE40; i++) {
            index = D_8004FE10++;
            slot = &D_8004FE2C[index];
            if (D_8004FE10 >= D_8004FE40) {
                D_8004FE10 = 0;
            }
            if (slot->state == 0) {
                break;
            }
        }
        if (slot->state == 0) {
            slot->state = 1;
            slot->sequence = D_8004FE26;
            D_8004FE26++;
            for (i = 0; i < 4; i++) {
                if (func_8004C398(D_80059F04, (u8 *)D_8004FE08 + index * 0x800, 0x800) != 0) {
                    break;
                }
                func_8002804C(i, 0, 0xFF, 0);
            }
            D_8004FDF8 -= 0x800;
            D_8004FE04++;
        } else {
            D_8004FDF8 = 0;
        }
        if (D_8004FDF8 > 0) {
            return;
        }
    }
    D_8004FDF8 = 0;
}
#else
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/main_8002A260", func_8002B8B0);
#endif


void func_8002BA40(void) {
    D_8004FDFC = D_8004FE00;
}

/* Mark the ring slot holding the next sector in order (D_8004FE28) as
 * complete; once the read has ended, stop the data callback and seek on. */
void func_8002BA58(void) {
    StreamSlot *slot = D_8004FE2C;
    s16 i;

    for (i = 0; i < D_8004FE40; i++, slot++) {
        if (slot->state == 1 && slot->sequence == D_8004FE28) {
            break;
        }
    }
    if (i != D_8004FE40) {
        slot->state = 3;
        D_8004FE28++;
        if (D_8004FDF8 <= 0 && D_8004FDFC < 2) {
            D_8004FDF8 = 0;
            CdDataCallback(NULL);
            func_8002A394(D_8004FE38);
            D_8004FDFC = 0;
        }
    }
}

/* Image stream step: take the ring slot holding the next sector in order.
 * A sector starting an image (type 0x1200/0x1201) gives its placement,
 * width and strip heights; each following sector is one strip, loaded to
 * VRAM. After the last strip of the last image the read is stopped. */
void func_8002BB50(void) {
    StreamSlot *slot = D_8004FE2C;
    s16 i;
    u32 *p;
    s32 type;
    u16 *pos;
    RECT rect;

    for (i = 0; i < D_8004FE40; i++, slot++) {
        if (slot->state == 1 && slot->sequence == D_8004FE28) {
            break;
        }
    }
    if (i == D_8004FE40) {
        return;
    }
    slot->state = 2;
    p = (u32 *)((u8 *)D_8004FE08 + i * 0x800);
    if (D_80059F50 == 0) {
        type = *p++;
        pos = (u16 *)p;
        if (type != 0x1200 && type != 0x1201) {
            goto end;
        }
        if (type == 0x1200) {
            switch (D_80059F24) {
            case 1:
                D_80059F40 = D_80059F28 + pos[2];
                D_80059F44 = D_80059F2C + pos[3];
                break;
            case 2:
                D_80059F40 = D_80059F28 + pos[0] + pos[2];
                D_80059F44 = D_80059F2C + pos[1] + pos[3];
                break;
            default:
                D_80059F40 = pos[0] + pos[2];
                D_80059F44 = pos[1] + pos[3];
                break;
            }
        }
        if (type == 0x1201) {
            switch (D_80059F30) {
            case 1:
                D_80059F40 = D_80059F34 + pos[2];
                D_80059F44 = D_80059F38 + pos[3];
                break;
            case 2:
                D_80059F40 = D_80059F34 + pos[0] + pos[2];
                D_80059F44 = D_80059F38 + pos[1] + pos[3];
                break;
            default:
                D_80059F40 = pos[0] + pos[2];
                D_80059F44 = pos[1] + pos[3];
                break;
            }
        }
        p += 2;
        D_80059F48 = *(u16 *)p;
        p += 2;
        if (D_80059F3C == 0) {
            D_80059F3C = *p;
        }
        p++;
        D_80059F50 = *p++;
        D_80059F4C = (u16 *)p;
    } else {
        rect.x = D_80059F40;
        rect.y = D_80059F44;
        rect.w = D_80059F48;
        rect.h = *D_80059F4C;
        LoadImage(&rect, (u_long *)p);
        D_80059F44 += *D_80059F4C++;
        if (--D_80059F50 <= 0) {
            D_80059F50 = 0;
            D_80059F3C--;
            for (i = 0; i < D_8004FE40; i++) {
                D_8004FE2C[i].state = 0;
                D_8004FE2C[i].sequence = 0;
            }
            if (D_80059F3C <= 0) {
            end:
                D_8004FDF8 = 0;
                CdDataCallback(NULL);
                func_8002A394(D_8004FE38);
                D_8004FDFC = 0;
                return;
            }
        }
        slot->state = 0;
    }
    D_8004FE28++;
}

/* Image stream step of PC file server reads (80029EB0): as 8002BB50, but
 * each strip load is waited for and the read simply ends. */
void func_8002BF38(void) {
    StreamSlot *slot = D_8004FE2C;
    s16 i;
    u32 *p;
    s32 type;
    u16 *pos;
    RECT rect;

    for (i = 0; i < D_8004FE40; i++, slot++) {
        if (slot->state == 1 && slot->sequence == D_8004FE28) {
            break;
        }
    }
    if (i == D_8004FE40) {
        return;
    }
    slot->state = 2;
    p = (u32 *)((u8 *)D_8004FE08 + i * 0x800);
    if (D_80059F50 == 0) {
        type = *p++;
        pos = (u16 *)p;
        if (type != 0x1200 && type != 0x1201) {
            return;
        }
        if (type == 0x1200) {
            switch (D_80059F24) {
            case 1:
                D_80059F40 = D_80059F28 + pos[2];
                D_80059F44 = D_80059F2C + pos[3];
                break;
            case 2:
                D_80059F40 = D_80059F28 + pos[0] + pos[2];
                D_80059F44 = D_80059F2C + pos[1] + pos[3];
                break;
            default:
                D_80059F40 = pos[0] + pos[2];
                D_80059F44 = pos[1] + pos[3];
                break;
            }
        }
        if (type == 0x1201) {
            switch (D_80059F30) {
            case 1:
                D_80059F40 = D_80059F34 + pos[2];
                D_80059F44 = D_80059F38 + pos[3];
                break;
            case 2:
                D_80059F40 = D_80059F34 + pos[0] + pos[2];
                D_80059F44 = D_80059F38 + pos[1] + pos[3];
                break;
            default:
                D_80059F40 = pos[0] + pos[2];
                D_80059F44 = pos[1] + pos[3];
                break;
            }
        }
        p += 2;
        D_80059F48 = *(u16 *)p;
        p += 2;
        if (D_80059F3C == 0) {
            D_80059F3C = *p;
        }
        p++;
        D_80059F50 = *p++;
        D_80059F4C = (u16 *)p;
    } else {
        rect.x = D_80059F40;
        rect.y = D_80059F44;
        rect.w = D_80059F48;
        rect.h = *D_80059F4C;
        LoadImage(&rect, (u_long *)p);
        DrawSync(0);
        D_80059F44 += *D_80059F4C++;
        if (--D_80059F50 <= 0) {
            D_80059F50 = 0;
            D_80059F3C--;
            for (i = 0; i < D_8004FE40; i++) {
                D_8004FE2C[i].state = 0;
                D_8004FE2C[i].sequence = 0;
            }
            if (D_80059F3C <= 0) {
                D_8004FDF8 = 0;
                D_8004FDFC = 0;
                return;
            }
        }
        slot->state = 0;
    }
    D_8004FE28++;
}

/* Print the image stream state (debug report). */
void func_8002C310(void) {
    func_800379C8("F%8x A%8x S%8x\n", D_8004FDF0, D_8004FE08, D_8004FE2C);
    func_800379C8("%d %d %d %d\n", D_8004FE40, D_8004FE10, D_80059F3C, D_80059F50);
    func_800379C8("%d %d %d %8x\n", D_80059F40, D_80059F44, D_80059F48, D_80059F4C);
    func_800379C8("%d %d %d\n", D_80059F4C[0], D_80059F4C[1], D_80059F4C[2]);
}

/* Nonzero when files come from the PC file server (its name table). */
s32 func_8002C3D8(void) {
    return (s32)D_8004FE48;
}
