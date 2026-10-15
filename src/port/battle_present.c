/*
 * Presentation records of a battle's stage (docs/runtime.md, Camera records;
 * present.c): read-only, nothing here writes game state. A diorama of a
 * battle stands it on its table where its combatants stand, facing the way
 * the battle's start view looks, for the whole battle, whatever its camera
 * does (it orbits, cuts and shakes); the records only reach the ring, which
 * no state holds, and only while a view from anywhere wants them.
 */
#include "common.h"
#include "xem/gte.h"
#include "resident/sprite.h"
#include "battle/area.h"
#include "battle/scene.h"

void xem_record_entry(u32 class_, s32 sz, u32 owner, s32 x, s32 y, s32 z);
/* Set once the battle's opening is over and its turns run (battle/command.h,
 * whose other headers the port does not need). */
extern u8 battle_turns_active;

/* Before the battle's camera is recorded (present.c sprite_set_view_matrix):
 * each combatant the battle shows, as its camera frames them
 * (battle_camera_frame_slots: a slot with a sprite, not hidden), its
 * sprite's place in the battle's world and its height, owned by its slot;
 * then the scene's start view, which battle_enter takes from the scene
 * data's camera 0: its look-at (its depth 1 once the battle's turns run:
 * its combatants have entered and stand, their heights set by the frames
 * they show), then its eye. Written to the ring only. */
void xem_present_battle_stage(void) {
    SceneCamera *start = &SCENE_DATA->cameras[0];
    Sprite *sprite;
    s32 i;

    for (i = 0; i < 11; i++) {
        sprite = BATTLE_AREA.sprites[i];
        if (sprite != NULL && !BATTLE_AREA.slots[i].hidden) {
            xem_record_entry(XEM_PROJECTION_STAGE, sprite->height, (u32)i, sprite->x >> 16, sprite->y >> 16, sprite->z >> 16);
        }
    }
    xem_record_entry(XEM_PROJECTION_STAGE, battle_turns_active != 0, XEM_STAGE_LOOK_AT, start->lookAt[0], start->lookAt[1],
                     start->lookAt[2]);
    xem_record_entry(XEM_PROJECTION_STAGE, 0, XEM_STAGE_EYE, start->eye[0], start->eye[1], start->eye[2]);
}
