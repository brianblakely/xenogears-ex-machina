/* The setup module's task (801e62e0 creates it, 801e7098 runs it) and the
 * enemy set loader (801e6314: sprite rows and image lists of the enemy set
 * file): text 801E62E0-801E70E8, data 801E95BC-801E963C, its pointer at
 * 801E96B4. A separate unit built by the Cygnus CDK GCC 2.7.2 with a later
 * ASPSX, between the GCC 2.6.3 units ovl2615.c and stage.c (ovl2615.mk). */
#include "loader.h"

/* The sprite file and sequencer word of each party member type. */
MemberFile D_801E95BC[] = {
    {1, 0x12}, {2, 0x13}, {4, 0x15},  {3, 0x14},  {5, 0x16}, {6, 0x17}, {7, 0x18},
    {8, 0x19}, {9, 0x1A}, {10, 0x1B}, {11, 0x1C}, {1, 0x12}, {1, 0x12}, {1, 0x12},
};
u8 D_801E962C[] = {16, 16, 16, 16, 16, 24, 16, 16, 16, 24, 16, 16};
/* D_801E9638 (u16 0, the next member image column) ends the unit's data
 * before stray bytes (0x00 0xe0), so it stays original data. */
INCLUDE_ORIGINAL(".data", D_801E9638, 0x801E9638, 4);
extern u16 D_801E9638; /* the next member image column */
/* Uninitialized: the overlay's file holds each unit's .bss after all .data. */
void *D_801E96B4;

/* Start the loading task for the enemy set file `data` while flagging the
 * battle setup as running. */
void func_801E62E0(u8 *data) {
    task_alloc_mode = 1;
    func_801E7098(data);
    task_alloc_mode = 0;
}

/* The enemy set file's sprite rows (slots 3..): its table is a count, a
 * first image column, then 12-byte entries. The data after the table is
 * copied; sprite entries upload their image list (or reuse an earlier one)
 * and fill their slot's row; model entries place the model on every enemy
 * slot of their type, and of later model entries sharing their data.
 * The placement mode reuses j (its unknown bits keep the andi 0xffff). */
void func_801E6314(u8 *data) {
    s32 count;
    s32 k;
    s32 images;
    s32 slot;
    u8 *base;
    s16 columns[16];
    s32 types[SLOT_COUNT];
    SVECTOR unused; /* unused in the original; reserves 8 bytes */
    EnemyEntry *entry;
    EnemyEntry *other;
    s32 table;
    s32 j, s, i;
    s32 column;
    u32 index;
    s32 type, same, placed;
    s32 enemy;

    images = 0;
    slot = 3;
    /* The original counts this loop with the column variable. */
    for (column = 0; column != SLOT_COUNT; column++) {
        types[column] = battle_area.slots[column].field2;
    }
    count = data[0];
    column = data[1] * 0x40 + 0x140;
    table = count * sizeof(EnemyEntry) + 8;
    battle_enemy_set_copy = heap_alloc(*(s32 *)(data + 4) - table, 0);
    memcpy(battle_enemy_set_copy, data + table, *(s32 *)(data + 4) - table);
    entry = (EnemyEntry *)(data + 8);
    base = (u8 *)battle_enemy_set_copy - table;
    for (k = 0; k != count; k++) {
        if (entry->model != 0) {
            placed = 0;
            if (entry->images < 8) {
                /* The entry pointers are not advanced here (as in the
                 * original). */
                continue;
            }
            type = slot - 3;
            battle_area.sources[slot].data = NULL;
            column += 0x40;
            for (j = 3, same = 0; j != SLOT_COUNT; j++) {
                if (type == types[j]) {
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
                if (types[s] < 8 && types[s] == type && (enemy = s) != 0) {
                    j = (same >= 2) ? 2 : 0;
                    if (placed != 0) {
                        j = 7;
                        if (same == placed + 1) {
                            j = 5;
                        }
                    }
                    j |= 0x80;
                    battle_create_object(enemy, (u16)j, (u8 *)(entry->offset + (s32)base),
                                  (u8 *)(entry->images + (s32)data), (s16)(column - 0x40), 0x100, 0,
                                  (s16)(enemy - (s16)(placed - 0x1C0)), 0);
                    placed++;
                    battle_object_follower_create(enemy);
                }
            }
        } else {
            if (entry->images < 8) {
                index = entry->images;
            } else {
                sprite_upload_images_side_by_side((u8 *)(entry->images + (s32)data), column, 0x100);
                index = images;
                columns[images++] = column;
                column += sprite_read_word((s32 *)(entry->images + (s32)data)) << 6;
                if (column >= 0x2C1) {
                    column = 0;
                }
            }
            battle_area.sources[slot].data = (u8 *)(entry->offset + (s32)base);
            battle_area.sources[slot].y = 0x100;
            battle_area.sources[slot].x = columns[index];
            battle_area.sources[slot].variant = entry->variant;
        }
        entry++;
        slot++;
    }
}

/* Each enemy slot's sprite from its type's row, when that row has data
 * (`data`, the enemy set file, is passed but unused). */
void func_801E6710(u8 *data) {
    s32 slot;
    s32 type;

    for (slot = 3; slot != SLOT_COUNT; slot++) {
        type = battle_area.slots[slot].field2;
        if (type < 8 && battle_area.sources[type + 3].data != NULL) {
            func_801E67A4(slot, type + 3, 1);
        }
    }
}

/* Create `slot`'s sprite task from sprite row `row` showing `animation`,
 * placed and facing as its placement record says. */
void func_801E67A4(s32 slot, s32 row, s32 animation) {
    Task *task;
    BattleSprite *sprite;
    s32 angle;

    task = battle_sprite_task_create(battle_area.sources[row].data, 0, slot + 0x1C0, battle_area.sources[row].x,
                         battle_area.sources[row].y, 0x20, 0, 0, 0, animation, 0, 0, 0,
                         battle_area.sources[row].variant);
    sprite = task->data;
    sprite->image[3] = battle_area.sources[row].y;
    sprite->image[2] = battle_area.sources[row].x;
    *(DVECTOR *)((u8 *)sprite->sequencer + 0xE) = *(DVECTOR *)&sprite->image[2];
    battle_area.sprites[slot] = task->data;
    battle_area.tasks[slot] = (SpriteTask *)task;
    sprite->slotLow = slot;
    sprite->slotHigh = (u32)slot >> 2;
    sprite_set_position_xz(sprite, battle_area.slots[slot].x, battle_area.slots[slot].z);
    angle = (battle_area.slots[slot].targetCode != 0) << 11;
    sprite_set_facing(sprite, angle);
    sprite_set_direction(sprite, angle);
    if (battle_area.slots[slot].hidden != 0) {
        sprite->countdown = 0;
    }
}

/* Read the party members' sprite files (directory 2c, file by type) into new
 * blocks, recording them in the members' rows, by the file list `list`. */
void func_801E693C(FileRequest *list) {
    s32 member, entries;
    s32 file;
    s32 type;
    void *block;

    cd_select_directory(0x2C, 1);
    for (entries = member = 0; member != 3; member++) {
        type = battle_area.slots[member].field2;
        if (type < 0x11 && battle_area.slots[member].gear == 0) {
            file = D_801E95BC[type].file;
            list[entries].file = file;
            block = heap_alloc(cd_get_aligned_file_size(file), 0);
            list[entries].destination = block;
            entries++;
            battle_area.sources[member].data = block;
            battle_area.sources[member].variant = 0;
        }
    }
    list[entries].file = 0;
    list[entries].destination = NULL;
    cd_read_file_list(list, 0, 0);
}

/* Set up the members placed with a model (800bb760). */
void func_801E6A4C(void) {
    s32 member;
    s32 x, y, z;
    s32 type;

    for (member = 0; member != 3; member++) {
        type = battle_area.slots[member].field2;
        if (type < 0x11 && battle_area.slots[member].gear != 0) {
            battle_gear_load_start(member);
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
        type = battle_area.slots[member].field2;
        if (type < 0x11 && battle_area.slots[member].gear == 0) {
            battle_area.sources[member].y = 0x1C0;
            battle_area.sources[member].x = D_801E9638 + 0x100;
            D_801E9638 += D_801E962C[type];
            func_801E67A4(member, member, 1);
            sprite = (BattleSprite *)battle_area.sprites[member];
            *sprite->sequencer = D_801E95BC[type].sequence;
            heap_free(sprite->renderer->parts[0]);
            sprite->renderer->parts[0] = heap_alloc(0x300, 0);
        }
    }
    if (battle_start_mode == 0) {
        for (member = 0; member != 3; member++) {
            sprite = (BattleSprite *)battle_area.sprites[member];
            if (sprite != NULL) {
                battle_sprite_update_ground((Sprite *)sprite);
                x = sprite->x;
                y = sprite->y;
                z = sprite->z;
                sprite->target[0] = x;
                sprite->target[1] = y;
                sprite->target[2] = z;
                sprite_start_animation((Sprite *)sprite, 0x17);
            }
        }
    }
}

/* Loading state: after the delay, wait until every member sprite has
 * reached the ground, then mark loading done and end the task. */
void func_801E6C80(Task *node) {
    LoaderTask *task = (LoaderTask *)node;
    BattleSprite *sprite;
    s32 member;

    if (task->timer == 0) {
        for (member = 0; member != 3; member++) {
            if (battle_area.slots[member].gear == 0 &&
                (sprite = (BattleSprite *)battle_area.sprites[member]) != NULL &&
                sprite->y != sprite->ground) {
                return;
            }
        }
        battle_area.field8DA8 = 1;
        task_destroy_main_task(node);
    } else {
        task->timer--;
    }
}

/* Loading state: once the members stop moving, wait 16 frames and settle. */
void func_801E6D34(Task *node) {
    if (battle_gear_object_load_count == 0) {
        ((LoaderTask *)node)->timer = 0x10;
        task_set_update_callback(node, func_801E6C80);
    }
}

/* Loading state: once the sound transfer is done, release the battle images
 * file and set up the members with models. */
void func_801E6D6C(Task *node) {
    if (sound_sync_transfer(0) == 0) {
        heap_free(((LoaderTask *)node)->images);
        func_801E6A4C();
        task_set_update_callback(node, func_801E6D34);
    }
}

/* Upload the battle images (D_801E96B4) on a private 8 KB stack. */
void func_801E6DC8(void) {
    u8 *stack = heap_alloc(0x2000, 1);

    /* Push the caller's sp at the new stack top and switch to it. */
    STACK_ENTER(stack + 0x1F00);
    model_load_image_list(D_801E96B4, 0, 0, 0, 0, 0, 0);
    DrawSync(0);
    STACK_LEAVE();
    heap_free(stack);
}

/* Loading state: once the disc is idle, upload the battle images, bind the
 * shared battle file (image 380,0, palette row 1d1) and link the effect
 * bank. */
void func_801E6E48(Task *node) {
    LoaderTask *task = (LoaderTask *)node;
    DVECTOR image;
    DVECTOR clut;

    if (cd_get_pending_read_count() == 0) {
        D_801E96B4 = task->images;
        func_801E6DC8();
        image.vx = 0x380;
        image.vy = 0;
        clut.vx = 0;
        clut.vy = 0x1D1;
        sprite_resolve_resource(sprite_shared_source, task->shared, image, clut, 0);
        sound_add_effect_bank(task->effects);
        sprite_script_sound_bank = task->effects;
        task_set_update_callback(node, func_801E6D6C);
        battle_enable_shadows();
    }
}

/* Loading state: once the member files are read, create the member sprites
 * and read battle files 1-3 (images, shared data, effects). */
void func_801E6F00(Task *node) {
    LoaderTask *task = (LoaderTask *)node;
    FileRequest *files;
    s32 busy;

    busy = cd_get_pending_read_count();
    cd_select_directory(0x2C, 0);
    if (busy == 0) {
        func_801E6AC4();
        files = task->files;
        task->images = files[0].destination = heap_alloc(cd_get_aligned_file_size(1), 1);
        files[0].file = 1;
        battle_file2_block_and_max_hp_digits = task->shared = files[1].destination = heap_alloc(cd_get_aligned_file_size(2), 0);
        files[1].file = 2;
        task->effects = files[2].destination = heap_alloc(cd_get_aligned_file_size(3), 0);
        files[2].file = 3;
        files[3].destination = NULL;
        files[3].file = 0;
        cd_read_file_list(files, 0, 0);
        task_set_update_callback(node, func_801E6E48);
    }
}

/* Loading state: once the disc is idle, build the enemy rows and sprites on
 * a private 16 KB stack, release the enemy set file and read the member
 * sprite files. */
void func_801E6FEC(Task *node) {
    LoaderTask *task = (LoaderTask *)node;
    u8 *stack;

    if (cd_get_pending_read_count() == 0) {
        stack = heap_alloc(0x4000, 1);
        STACK_ENTER(stack + 0x3FFC);
        func_801E6314(task->data);
        func_801E6710(task->data);
        STACK_LEAVE();
        heap_free(stack);
        DrawSync(0);
        heap_free(task->data);
        task_set_update_callback(node, func_801E6F00);
        func_801E693C(task->members);
    }
}

/* Create the loading task for the enemy set file `data`. */
void func_801E7098(u8 *data) {
    LoaderTask *task = (LoaderTask *)task_alloc_main_task(0, 0x78);

    task_set_update_callback(&task->task, func_801E6FEC);
    task->data = data;
}
