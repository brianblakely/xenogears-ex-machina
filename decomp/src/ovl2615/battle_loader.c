/* The setup module's task (801e62e0 creates it, 801e7098 runs it) and the
 * enemy set loader (801e6314: sprite rows and image lists of the enemy set
 * file). A separate unit built by the Cygnus CDK GCC 2.7.2
 * (see ovl2615.mk). */
#include "battle_setup.h"

/* Start the loading task for the enemy set file `data` while flagging the
 * battle setup as running. */
void func_801E62E0(u8 *data) {
    D_800591AF = 1;
    func_801E7098(data);
    D_800591AF = 0;
}

/* The enemy set file's sprite rows (slots 3..): its table is a count, a
 * first image column, then 12-byte entries. The data after the table is
 * copied; sprite entries upload their image list (or reuse an earlier one)
 * and fill their slot's row; model entries place the model on every enemy
 * slot of their type, and of later model entries sharing their data. */
#ifdef NON_MATCHING
void func_801E6314(u8 *data) {
    s16 columns[8];
    s32 types[SLOT_COUNT];
    EnemyEntry *entry;
    EnemyEntry *other;
    SpriteRow *row;
    u8 *base;
    s32 count, table, size;
    s32 images, slot, k, j, s;
    s32 column;
    u32 index;
    s32 type, same, placed, many, mode;
    s16 y;

    for (s = 0; s != SLOT_COUNT; s++) {
        types[s] = D_800C3EB0.slots[s].id;
    }
    images = 0;
    slot = 3;
    count = data[0];
    column = data[1] * 0x40 + 0x140;
    table = count * sizeof(EnemyEntry) + 8;
    size = *(s32 *)(data + 4) - table;
    D_800D39C8 = func_80031BDC(size, 0);
    memcpy(D_800D39C8, data + table, *(s32 *)(data + 4) - table);
    entry = (EnemyEntry *)(data + 8);
    base = (u8 *)D_800D39C8 - table;
    for (k = 0; k != count; k++) {
        row = &D_800C3EB0.rows[slot];
        if (entry->model != 0) {
            placed = 0;
            if (entry->images < 8) {
                /* The entry pointers are not advanced here (as in the
                 * original). */
                continue;
            }
            column += 0x40;
            same = 0;
            type = slot - 3;
            row->data = NULL;
            for (s = 3; s != SLOT_COUNT; s++) {
                if (types[s] == type) {
                    same++;
                }
            }
            other = entry + 1;
            for (j = k + 1; j != count; j++, other++) {
                if (other->model != 0 && other->images == k) {
                    for (s = 3; s != SLOT_COUNT; s++) {
                        if (types[s] == j) {
                            types[s] = type;
                            same++;
                        }
                    }
                }
            }
            many = -(same >= 2);
            y = column - 0x40;
            for (s = 3; s != SLOT_COUNT; s++) {
                if (types[s] < 8 && types[s] == type && s != 0) {
                    mode = many & 2;
                    if (placed != 0) {
                        mode = 7;
                        if (same == placed + 1) {
                            mode = 5;
                        }
                    }
                    func_800A8BF0(s, (mode | 0x80) & 0xFFFF, base + entry->offset,
                                  data + entry->images, y, 0x100, 0,
                                  (s16)(s - (placed - 0x1C0)), 0);
                    placed++;
                    func_800BB350(s);
                }
            }
        } else {
            index = entry->images;
            if (index >= 8) {
                func_80022A70(data + index, column, 0x100);
                index = images;
                columns[images++] = column;
                column += func_80022A00(data + entry->images) << 6;
                if (column >= 0x2C1) {
                    column = 0;
                }
            }
            row->y = 0x100;
            row->data = base + entry->offset;
            row->x = columns[index];
            row->variant = entry->variant;
        }
        entry++;
        slot++;
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/battle_loader", func_801E6314);
#endif

/* Each enemy slot's sprite from its type's row, when that row has data. */
#ifdef NON_MATCHING
void func_801E6710(void) {
    s32 slot;
    u8 type;

    for (slot = 3; slot != SLOT_COUNT; slot++) {
        type = D_800C3EB0.slots[slot].id;
        if (type < 8 && D_800C3EB0.rows[type + 3].data != NULL) {
            func_801E67A4(slot, type + 3, 1);
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/battle_loader", func_801E6710);
#endif

/* Create `slot`'s sprite task from sprite row `row` showing `animation`,
 * placed and facing as its placement record says. */
#ifdef NON_MATCHING
void func_801E67A4(s32 slot, s32 row, s32 animation) {
    SpriteRow *source = &D_800C3EB0.rows[row];
    SlotInfo *info;
    BattleSprite **task;
    BattleSprite *sprite;
    s32 angle;

    task = func_800BA984(source->data, 0, slot + 0x1C0, source->x, source->y, 0x20, 0, 0, 0,
                         animation, 0, 0, 0, source->variant);
    sprite = task[1];
    sprite->binding[3] = source->y;
    sprite->binding[2] = source->x;
    info = &D_800C3EB0.slots[slot];
    *(u32 *)((u8 *)sprite->sequence + 0xE) = *(u32 *)&sprite->binding[2];
    D_800C3EB0.sprites[slot] = task[1];
    D_800C3EB0.tasks[slot] = (TaskNode *)task;
    sprite->flagsA8 = (sprite->flagsA8 & 0x3FFFFFFF) | (slot << 30);
    sprite->flagsAC = (sprite->flagsAC & ~3) | ((slot >> 2) & 3);
    func_80021D3C(sprite, info->x, info->z);
    angle = (info->flag6 != 0) << 11;
    func_800223B0(sprite, angle);
    func_80021FE0(sprite, angle);
    if (info->flag3 != 0) {
        sprite->v9E = 0;
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/battle_loader", func_801E67A4);
#endif

/* Read the party members' sprite files (directory 2c, file by type) into new
 * blocks, recording them in the members' rows, by the file list `list`. */
#ifdef NON_MATCHING
void func_801E693C(FileEntry *list) {
    FileEntry *entry = list;
    s32 member, entries = 0;
    s32 file;
    u8 type;
    void *block;

    func_80028470(0x2C, 1);
    for (member = 0; member != 3; member++) {
        type = D_800C3EB0.slots[member].id;
        if (type < 0x11 && D_800C3EB0.slots[member].alone == 0) {
            file = D_801E95BC[type].file;
            entries++;
            entry->file = file;
            block = func_80031BDC(func_800288EC(file), 0);
            entry->dest = block;
            entry++;
            D_800C3EB0.rows[member].data = block;
            D_800C3EB0.rows[member].variant = 0;
        }
    }
    list[entries].file = 0;
    list[entries].dest = NULL;
    func_80029AFC((u16 *)list, 0, 0);
}
#else
INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/battle_loader", func_801E693C);
#endif

/* Set up the members placed with a model (800bb760). */
#ifdef NON_MATCHING
void func_801E6A4C(void) {
    s32 member;

    for (member = 0; member != 3; member++) {
        if (D_800C3EB0.slots[member].id < 0x11 && D_800C3EB0.slots[member].alone != 0) {
            func_800BB760(member);
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/battle_loader", func_801E6A4C);
#endif

/* Each party member's sprite: its row takes the next image columns at row
 * 1c0, its sequencer word and a fresh 0x300-byte part block; then (unless
 * 800d36b8) the members are put on the stage floor facing their home. */
#ifdef NON_MATCHING
void func_801E6AC4(void) {
    BattleSprite *sprite;
    s32 member;
    u8 type;

    for (member = 0; member != 3; member++) {
        type = D_800C3EB0.slots[member].id;
        if (type < 0x11 && D_800C3EB0.slots[member].alone == 0) {
            D_800C3EB0.rows[member].y = 0x1C0;
            D_800C3EB0.rows[member].x = D_801E9638 + 0x100;
            D_801E9638 += D_801E962C[type];
            func_801E67A4(member, member, 1);
            sprite = D_800C3EB0.sprites[member];
            *sprite->sequence = D_801E95BC[type].sequence;
            func_800320E8(sprite->renderer->parts);
            sprite->renderer->parts = func_80031BDC(0x300, 0);
        }
    }
    if (D_800D36B8 == 0) {
        for (member = 0; member != 3; member++) {
            sprite = D_800C3EB0.sprites[member];
            if (sprite != NULL) {
                func_800BA8F4(sprite);
                sprite->home[0] = sprite->x;
                sprite->home[1] = sprite->y;
                sprite->home[2] = sprite->z;
                func_800245D8(sprite, 0x17, sprite->z);
            }
        }
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/battle_loader", func_801E6AC4);
#endif

/* Loading state: after the delay, wait until every member sprite has
 * reached the ground, then mark loading done and end the task. */
#ifdef NON_MATCHING
void func_801E6C80(TaskNode *node) {
    LoaderTask *task = (LoaderTask *)node;
    BattleSprite *sprite;
    s32 member;

    if (task->timer != 0) {
        task->timer--;
        return;
    }
    for (member = 0; member != 3; member++) {
        if (D_800C3EB0.slots[member].alone == 0 && (sprite = D_800C3EB0.sprites[member]) != NULL &&
            sprite->y != sprite->ground) {
            return;
        }
    }
    D_800C3EB0.loaded = 1;
    func_8001CE44(node);
}
#else
INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/battle_loader", func_801E6C80);
#endif

/* Loading state: once the members stop moving, wait 16 frames and settle. */
void func_801E6D34(TaskNode *node) {
    if (D_800C35D8 == 0) {
        ((LoaderTask *)node)->timer = 0x10;
        func_8001CD6C(node, func_801E6C80);
    }
}

/* Loading state: once the sound transfer is done, release the battle images
 * file and set up the members with models. */
void func_801E6D6C(TaskNode *node) {
    if (func_8003BDFC(0) == 0) {
        func_800320E8(((LoaderTask *)node)->images);
        func_801E6A4C();
        func_8001CD6C(node, func_801E6D34);
    }
}

/* Upload the battle images (D_801E96B4) on a private 8 KB stack. */
void func_801E6DC8(void) {
    u8 *stack = func_80031BDC(0x2000, 1);

    /* Push the caller's sp at the new stack top and switch to it. */
    __asm__ volatile("move $8, %0\n\tsw $29, 0($8)\n\taddiu $8, $8, -4\n\tmove $29, $8"
                     :
                     : "r"(stack + 0x1F00)
                     : "$8", "memory");
    func_8002DDE4(D_801E96B4, 0, 0, 0, 0, 0, 0);
    DrawSync(0);
    __asm__ volatile("addiu $29, $29, 4\n\tlw $29, 0($29)" : : : "memory");
    func_800320E8(stack);
}

/* Loading state: once the disc is idle, upload the battle images, bind the
 * shared battle file (image 380,0, palette row 1d1) and link the effect
 * bank. */
void func_801E6E48(TaskNode *node) {
    LoaderTask *task = (LoaderTask *)node;
    Point image;
    Point clut;

    if (func_800286CC() == 0) {
        D_801E96B4 = task->images;
        func_801E6DC8();
        image.x = 0x380;
        image.y = 0;
        clut.x = 0;
        clut.y = 0x1D1;
        func_80022224(D_8006BE10, task->shared, image, clut, 0);
        func_80038428(task->effects);
        D_8005919C = task->effects;
        func_8001CD6C(node, func_801E6D6C);
        func_800B14B8();
    }
}

/* Loading state: once the member files are read, create the member sprites
 * and read battle files 1-3 (images, shared data, effects). */
#ifdef NON_MATCHING
void func_801E6F00(TaskNode *node) {
    LoaderTask *task = (LoaderTask *)node;
    s32 busy;

    busy = func_800286CC();
    func_80028470(0x2C, 0);
    if (busy == 0) {
        func_801E6AC4();
        task->files[0].dest = task->images = func_80031BDC(func_800288EC(1), 1);
        task->files[0].file = 1;
        D_800D2D54 = task->files[1].dest = task->shared = func_80031BDC(func_800288EC(2), 0);
        task->files[1].file = 2;
        task->files[2].dest = task->effects = func_80031BDC(func_800288EC(3), 0);
        task->files[2].file = 3;
        task->files[3].dest = NULL;
        task->files[3].file = 0;
        func_80029AFC((u16 *)task->files, 0, 0);
        func_8001CD6C(node, func_801E6E48);
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/battle_loader", func_801E6F00);
#endif

/* Loading state: once the disc is idle, build the enemy rows and sprites on
 * a private 16 KB stack, release the enemy set file and read the member
 * sprite files. */
#ifdef NON_MATCHING
void func_801E6FEC(TaskNode *node) {
    LoaderTask *task = (LoaderTask *)node;
    u8 *stack;

    if (func_800286CC() == 0) {
        stack = func_80031BDC(0x4000, 1);
        __asm__ volatile("move $8, %0\n\tsw $29, 0($8)\n\taddiu $8, $8, -4\n\tmove $29, $8"
                         :
                         : "r"(stack + 0x3FFC)
                         : "$8", "memory");
        func_801E6314(task->data);
        func_801E6710();
        __asm__ volatile("addiu $29, $29, 4\n\tlw $29, 0($29)" : : : "memory");
        func_800320E8(stack);
        DrawSync(0);
        func_800320E8(task->data);
        func_8001CD6C(node, func_801E6F00);
        func_801E693C(task->members);
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/battle_loader", func_801E6FEC);
#endif

/* Create the loading task for the enemy set file `data`. */
void func_801E7098(u8 *data) {
    LoaderTask *task = func_8001CD08(0, 0x78);

    func_8001CD6C(&task->task, func_801E6FEC);
    task->data = data;
}
