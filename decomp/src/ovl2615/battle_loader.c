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
    s32 count;
    s32 k;
    s32 images;
    s32 slot;
    u8 *base;
    s16 columns[16];
    s32 types[SLOT_COUNT];
    EnemyEntry *entry;
    EnemyEntry *other;
    s32 table;
    s32 j, s, i;
    s32 column;
    u32 index;
    s32 type, same, placed, mode;
    s32 enemy;

    images = 0;
    slot = 3;
    /* The original counts this loop with the column variable. */
    for (column = 0; column != SLOT_COUNT; column++) {
        types[column] = D_800C3EB0.slots[column].id;
    }
    count = data[0];
    column = data[1] * 0x40 + 0x140;
    table = count * sizeof(EnemyEntry) + 8;
    D_800D39C8 = func_80031BDC(*(s32 *)(data + 4) - table, 0);
    memcpy(D_800D39C8, data + table, *(s32 *)(data + 4) - table);
    entry = (EnemyEntry *)(data + 8);
    base = (u8 *)D_800D39C8 - table;
    for (k = 0; k != count; k++) {
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
            D_800C3EB0.rows[slot].data = NULL;
            for (i = 3; i != SLOT_COUNT; i++) {
                if (type == types[i]) {
                    same++;
                }
            }
            other = entry + 1;
            for (j = k + 1; j != count; j++, other++) {
                if (other->model != 0 && other->images == k) {
                    for (i = 3; i != SLOT_COUNT; i++) {
                        if (j == types[i]) {
                            types[i] = type;
                            same++;
                        }
                    }
                }
            }
            for (s = 3; s != SLOT_COUNT; s++) {
                if (types[s] < 8 && types[s] == type && s != 0) {
                    enemy = s;
                    mode = (same >= 2) ? 2 : 0;
                    if (placed != 0) {
                        mode = 7;
                        if (same == placed + 1) {
                            mode = 5;
                        }
                    }
                    func_800A8BF0(enemy, (u16)(mode | 0x80), (u8 *)(entry->offset + (s32)base),
                                  (u8 *)(entry->images + (s32)data), (s16)(column - 0x40), 0x100, 0,
                                  (s16)(enemy - (s16)(placed++ - 0x1C0)), 0);
                    func_800BB350(enemy);
                }
            }
        } else {
            index = entry->images;
            if (index >= 8) {
                func_80022A70((u8 *)(index + (s32)data), column, 0x100);
                index = images;
                columns[images++] = column;
                column += func_80022A00((u8 *)(entry->images + (s32)data)) << 6;
                if (column >= 0x2C1) {
                    column = 0;
                }
            }
            D_800C3EB0.rows[slot].y = 0x100;
            D_800C3EB0.rows[slot].data = (u8 *)(entry->offset + (s32)base);
            D_800C3EB0.rows[slot].x = columns[index];
            D_800C3EB0.rows[slot].variant = entry->variant;
        }
        entry++;
        slot++;
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2615/asm/nonmatchings/battle_loader", func_801E6314);
#endif

/* Each enemy slot's sprite from its type's row, when that row has data
 * (`data`, the enemy set file, is passed but unused). */
void func_801E6710(u8 *data) {
    s32 slot;
    s32 type;

    for (slot = 3; slot != SLOT_COUNT; slot++) {
        type = D_800C3EB0.slots[slot].id;
        if (type < 8 && D_800C3EB0.rows[type + 3].data != NULL) {
            func_801E67A4(slot, type + 3, 1);
        }
    }
}

/* Create `slot`'s sprite task from sprite row `row` showing `animation`,
 * placed and facing as its placement record says. */
void func_801E67A4(s32 slot, s32 row, s32 animation) {
    BattleSprite **task;
    BattleSprite *sprite;
    s32 angle;

    task = func_800BA984(D_800C3EB0.rows[row].data, 0, slot + 0x1C0, D_800C3EB0.rows[row].x,
                         D_800C3EB0.rows[row].y, 0x20, 0, 0, 0, animation, 0, 0, 0,
                         D_800C3EB0.rows[row].variant);
    sprite = task[1];
    sprite->binding[3] = D_800C3EB0.rows[row].y;
    sprite->binding[2] = D_800C3EB0.rows[row].x;
    *(Point *)((u8 *)sprite->sequence + 0xE) = *(Point *)&sprite->binding[2];
    D_800C3EB0.sprites[slot] = task[1];
    D_800C3EB0.tasks[slot] = (TaskNode *)task;
    sprite->slotLow = slot;
    sprite->slotHigh = (u32)slot >> 2;
    func_80021D3C(sprite, D_800C3EB0.slots[slot].x, D_800C3EB0.slots[slot].z);
    angle = (D_800C3EB0.slots[slot].flag6 != 0) << 11;
    func_800223B0(sprite, angle);
    func_80021FE0(sprite, angle);
    if (D_800C3EB0.slots[slot].flag3 != 0) {
        sprite->v9E = 0;
    }
}

/* Read the party members' sprite files (directory 2c, file by type) into new
 * blocks, recording them in the members' rows, by the file list `list`. */
void func_801E693C(FileEntry *list) {
    s32 member, entries;
    s32 file;
    s32 type;
    void *block;

    func_80028470(0x2C, 1);
    for (entries = member = 0; member != 3; member++) {
        type = D_800C3EB0.slots[member].id;
        if (type < 0x11 && D_800C3EB0.slots[member].alone == 0) {
            file = D_801E95BC[type].file;
            list[entries].file = file;
            block = func_80031BDC(func_800288EC(file), 0);
            list[entries].dest = block;
            entries++;
            D_800C3EB0.rows[member].data = block;
            D_800C3EB0.rows[member].variant = 0;
        }
    }
    list[entries].file = 0;
    list[entries].dest = NULL;
    func_80029AFC((u16 *)list, 0, 0);
}

/* Set up the members placed with a model (800bb760). */
void func_801E6A4C(void) {
    s32 member;
    s32 x, y, z;
    s32 type;

    for (member = 0; member != 3; member++) {
        type = D_800C3EB0.slots[member].id;
        if (type < 0x11 && D_800C3EB0.slots[member].alone != 0) {
            func_800BB760(member);
        }
    }
}

/* Each party member's sprite: its row takes the next image columns at row
 * 1c0, its sequencer word and a fresh 0x300-byte part block; then (unless
 * 800d36b8) the members are put on the stage floor facing their home. */
void func_801E6AC4(void) {
    BattleSprite *sprite;
    s32 member;
    s32 x, y, z;
    s32 type;

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
                x = sprite->x;
                y = sprite->y;
                z = sprite->z;
                sprite->home[0] = x;
                sprite->home[1] = y;
                sprite->home[2] = z;
                func_800245D8(sprite, 0x17);
            }
        }
    }
}

/* Loading state: after the delay, wait until every member sprite has
 * reached the ground, then mark loading done and end the task. */
void func_801E6C80(TaskNode *node) {
    LoaderTask *task = (LoaderTask *)node;
    BattleSprite *sprite;
    s32 member;

    if (task->timer == 0) {
        for (member = 0; member != 3; member++) {
            if (D_800C3EB0.slots[member].alone == 0 &&
                (sprite = D_800C3EB0.sprites[member]) != NULL && sprite->y != sprite->ground) {
                return;
            }
        }
        D_800C3EB0.loaded = 1;
        func_8001CE44(node);
    } else {
        task->timer--;
    }
}

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
void func_801E6F00(TaskNode *node) {
    LoaderTask *task = (LoaderTask *)node;
    FileEntry *files;
    s32 busy;

    busy = func_800286CC();
    func_80028470(0x2C, 0);
    if (busy == 0) {
        func_801E6AC4();
        files = task->files;
        task->images = files[0].dest = func_80031BDC(func_800288EC(1), 1);
        files[0].file = 1;
        D_800D2D54 = task->shared = files[1].dest = func_80031BDC(func_800288EC(2), 0);
        files[1].file = 2;
        task->effects = files[2].dest = func_80031BDC(func_800288EC(3), 0);
        files[2].file = 3;
        files[3].dest = NULL;
        files[3].file = 0;
        func_80029AFC((u16 *)files, 0, 0);
        func_8001CD6C(node, func_801E6E48);
    }
}

/* Loading state: once the disc is idle, build the enemy rows and sprites on
 * a private 16 KB stack, release the enemy set file and read the member
 * sprite files. */
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
        func_801E6710(task->data);
        __asm__ volatile("addiu $29, $29, 4\n\tlw $29, 0($29)" : : : "memory");
        func_800320E8(stack);
        DrawSync(0);
        func_800320E8(task->data);
        func_8001CD6C(node, func_801E6F00);
        func_801E693C(task->members);
    }
}

/* Create the loading task for the enemy set file `data`. */
void func_801E7098(u8 *data) {
    LoaderTask *task = func_8001CD08(0, 0x78);

    func_8001CD6C(&task->task, func_801E6FEC);
    task->data = data;
}
