/* Field unit 8006FDEC-8007A44C: the field load, the frame and its passes
 * (models, sprites, compass), fades, camera, and the field mode entry.
 * Its rodata runs 0x0-0x9c (the overlay number first); where its text ends
 * is chosen with the next unit (see field_8007A44C.c). */
#include "common.h"
#include "field.h"
#include "field_anim.h"
#include "field_gte.h"
#include "field_motion.h"
#include "field_music.h"

/* The overlay's number, ahead of this unit's other rodata (the field is
 * mode overlay 4). */
const s32 D_8006FAF0 = 4;

/* The field's shared state, defined here in the original order. The words
 * this unit does not read are used by the other units and the debug monitor
 * (debug595); the music table's copy above opens this data. */
s32 D_800ADAFC = 0;     /* debug monitor page toggle (debug595) */
u16 D_800ADB00 = 0xFFFF;
s16 D_800ADB02 = 0;     /* frames stuck against terrain */
u8 D_800ADB04 = 0;      /* random encounters enabled */
u8 D_800ADB05 = 0;      /* 1 while character drawing is off */
s32 D_800ADB08 = 0;     /* current draw buffer */
s32 D_800ADB0C = 0;
void *D_800ADB10 = NULL; /* first portrait image */
void *D_800ADB14 = NULL; /* second portrait image */
s32 D_800ADB18 = 0;
s32 D_800ADB1C = 0;     /* 801e module loaded */
void *D_800ADB20 = NULL; /* the 801e module */
s32 D_800ADB24 = 0;     /* screen effect buffers allocated */
s32 D_800ADB28 = 0;     /* latched jump setting */
s32 D_800ADB2C = 0;
u32 D_800ADB30 = 0;     /* heap top */
s32 D_800ADB34 = 0;
s32 D_800ADB38 = 0;     /* requested transition */
s32 D_800ADB3C = 0;     /* transition operand */
s32 D_800ADB40 = 0xFF;
s32 D_800ADB44 = 0;     /* last effect owner */
s16 D_800ADB48 = 0;     /* needle heading */
s16 D_800ADB4A = 0;     /* needle goal */
s32 D_800ADB4C = 0;
s32 D_800ADB50 = 0;
s16 D_800ADB54 = 0;
s32 D_800ADB58 = 0;     /* descriptor whose list is read */
s32 D_800ADB5C = 0;     /* list position */
s32 D_800ADB60 = 0;     /* field stream running */
s32 D_800ADB64 = 0xFF;  /* requested menu (scripts 0-6, menu button 0x80), 0xff none */
s32 D_800ADB68 = 0;     /* pad input polled this pass */
s32 D_800ADB6C = 0;     /* movie stopped */
s32 D_800ADB70 = 0;     /* movie requested */
s32 D_800ADB74 = 0;     /* movie mode */
s32 D_800ADB78 = 0;
s32 D_800ADB7C = 0;
s32 D_800ADB80 = 0;
s32 D_800ADB84 = 0;
s32 D_800ADB88 = 0;
s32 D_800ADB8C = 0;
s32 D_800ADB90 = 0;
s32 D_800ADB94 = 0;     /* camera distance */
s32 D_800ADB98 = 0;
s32 D_800ADB9C = 0;     /* frame start time */
s32 D_800ADBA0 = 0;     /* frame draw (CPU) time */
s32 D_800ADBA4 = 0;     /* GPU time */
s32 D_800ADBA8 = 0;
s32 D_800ADBAC = 0;     /* camera frames settling */
s32 D_800ADBB0 = 0;     /* camera frames releasing */
s32 D_800ADBB4 = 0;
void *D_800ADBB8 = NULL; /* music-wave stream ring */
s32 D_800ADBBC = 0;     /* stream arrivals */
void *D_800ADBC0 = NULL; /* pending party sprite buffer */
s32 D_800ADBC4 = 0xFF;
s32 D_800ADBC8 = 0;
s32 D_800ADBCC = 0;     /* pending party slot */
s32 D_800ADBD0 = 0;
s32 D_800ADBD4 = 0;
s32 D_800ADBD8 = 0;
s32 D_800ADBDC = 0;
s32 D_800ADBE0 = 0;
s32 D_800ADBE4 = 0;
s32 D_800ADBE8 = 0;
s32 D_800ADBEC = 0;     /* publish the field id on the next walk */
void *D_800ADBF0 = NULL; /* field message table */
Zone *D_800ADBF4 = NULL; /* trigger zones */
EventPackage *D_800ADBF8 = NULL;
s32 D_800ADBFC = 0;     /* event actor count */
u8 *D_800ADC00 = NULL;  /* event bytecode */
s32 D_800ADC04 = 2;     /* fade mode; fades start only in mode 2 */
s16 D_800ADC08 = 1;     /* fade started */
s32 D_800ADC0C = 0;
s32 D_800ADC10 = 0;     /* scratchpad words in use */
void *D_800ADC14 = NULL; /* field stream ring */
s32 D_800ADC18 = 0;

/* Octant bits. */
u8 D_800ADC1C[8] = {0x10, 0x20, 0x40, 0x80, 0x01, 0x02, 0x04, 0x08};

/* The compass: the heading octant bit of each palette row and the letters'
 * x, z offsets. */
u16 D_800ADC24[8] = {0x81, 0xC0, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x03};
DVECTOR D_800ADC34[4] = {{0, 0x500}, {0x500, 0}, {0, -0x500}, {-0x500, 0}};

/* Where the text images go: x, y, palette x, y, w, h per image. Nine rows;
 * 80077620 loads the first eight. */
s16 D_800ADC44[9 * 6] = {
    0x2A0, 0x1C0, 0,     0xFB, 0,    0,
    0x280, 0x1E0, 0x100, 0xF3, 0x10, 1,
    0x29C, 0x1C0, 0x100, 0xF5, 0,    0,
    0x280, 0x1C0, 0x100, 0xF2, 0,    0,
    0x280, 0x1F0, 0x100, 0xF4, 0x10, 1,
    0x3C0, 0x140, 0x100, 0xF7, 0x10, 1,
    0x298, 0x1C0, 0x100, 0xF6, 0x10, 1,
    0x288, 0x1C0, 0x100, 0xF6, 0x10, 1,
    0x380, 0x100, 0,     0xE8, 0x10, 1,
};

/* The VRAM blocks the menu overwrites (x, y pairs) and where they are saved
 * meanwhile. */
s16 D_800ADCB0[12] = {0, 0xE0, 0x40, 0xE0, 0x80, 0xE0, 0xC0, 0xE0, 0x100, 0xE0, 0x100, 0x1E0};
s16 D_800ADCC8[12] = {0x2C0, 0, 0x2C0, 0x20, 0x2C0, 0x40, 0x2C0, 0x60, 0x2C0, 0x80, 0x2C0, 0xA0};

/* Build the camera matrix from the eye, target and up vectors, the world
 * matrix under it, then the three lights and background color from the
 * field's view record, and the light matrix under the world matrix. */
void func_8006FDEC(s16 *record) {
    s32 flag;

    func_80073750(&D_800AF880.previous_view, &D_800AF880.eye, &D_800AF880.target, &D_800AF880.up);
    func_8003F738(&D_800AF880.world_angles, &D_800AF880.scaled_world);
    MulMatrix2(&D_800AF880.previous_view, &D_800AF880.scaled_world);

    D_800AF880.lights[0].direction[0] = *record++;
    D_800AF880.lights[0].direction[1] = *record++;
    D_800AF880.lights[0].direction[2] = *record;
    record += 2;
    D_800AF880.lights[0].color[0] = *record++ << 3;
    D_800AF880.lights[0].color[1] = *record++ << 3;
    D_800AF880.lights[0].color[2] = *record << 3;
    record += 2;
    func_80030A30(0, &D_800AF880.lights[0]);

    D_800AF880.lights[1].direction[0] = *record++;
    D_800AF880.lights[1].direction[1] = *record++;
    D_800AF880.lights[1].direction[2] = *record;
    record += 2;
    D_800AF880.lights[1].color[0] = *record++ << 3;
    D_800AF880.lights[1].color[1] = *record++ << 3;
    D_800AF880.lights[1].color[2] = *record << 3;
    record += 2;
    func_80030A30(1, &D_800AF880.lights[1]);

    D_800AF880.lights[2].direction[0] = *record++;
    D_800AF880.lights[2].direction[1] = *record++;
    D_800AF880.lights[2].direction[2] = *record;
    record += 2;
    D_800AF880.lights[2].color[0] = *record++ << 3;
    D_800AF880.lights[2].color[1] = *record++ << 3;
    D_800AF880.lights[2].color[2] = *record << 3;
    D_800AF880.lights[1] = D_800AF880.lights[0];
    D_800AF880.lights[2] = D_800AF880.lights[0];
    record += 2;
    func_80030A30(2, &D_800AF880.lights[2]);

    D_800AF880.back_color[0] = record[0] << 4;
    D_800AF880.back_color[1] = record[1] << 4;
    D_800AF880.back_color[2] = record[2] << 4;
    SetRotMatrix(&D_800AF880.previous_view);
    SetTransMatrix(&D_800AF880.previous_view);
    func_8004A6DC(&D_800AF880.anchor, D_800AF880.scaled_world.t, &flag);
    func_80030B14(&D_800AF880.scaled_world);
    SetRotMatrix(&D_800AF880.scaled_world);
    SetTransMatrix(&D_800AF880.scaled_world);
}

/* Decode compressed `source` data into `destination`. */
void func_8007008C(s32 unused, void *source, void *destination) {
    func_80032EB4(source, destination);
}

/* Tear the field down: reset the GPU, flush both draw buffers, then release
 * every model instance, the loaded components, the text windows and the
 * 801e module's buffers. */
void func_800700B0(void) {
    s32 i;
    s32 next;
    FieldInstance *instance;
    s32 *module_loaded;

    ResetGraph(1);
    func_8001C8DC();
    for (i = 0; i < 2;) {
        func_80025044();
        DrawSync(0);
        next = i + 1;
        func_800250E0((D_800ADB08 + next) & 1);
        i = next;
        func_80025044();
        DrawSync(0);
        func_80024FB8();
    }

    for (i = 0; i < D_800AF880.components.descriptor_count; i++) {
        func_8008083C(i);
        if (!(D_800AF880.components.descriptors[i].flags & 0x40)) {
            instance = D_800AF880.components.descriptors[i].instance;
            if (D_800AF880.components.descriptors[i].flags & 0x2000) {
                func_800306D0(instance->anims);
            }
            func_8002CBBC(instance->mesh);
            func_800320E8(instance->packets[0]);
            func_800320E8(D_800AF880.components.descriptors[i].instance);
        }
    }
    func_800A47D4();
    func_800320E8(D_800AF880.components.descriptors);
    func_800320E8(D_800ADBF4);
    func_800320E8(D_800ADBF0);
    func_800320E8(D_800ADBF8);
    func_800320E8(D_800AF880.components.collision);
    func_800320E8(D_800AF880.components.geometry);
    func_800320E8(D_800AF880.components.sprites);
    if (D_800B0080.enabled != 0) {
        func_80027D40(D_800B007C);
    }
    for (i = 0; i < D_800AFEA8.count; i++) {
        func_8002800C(D_800AFEA8.handles[i]);
        func_800320E8(D_800AFEA8.buffers[i]);
        func_800320E8((void *)D_800AFEA8.handles[i]);
    }
    func_8003748C();
    module_loaded = &D_800B2264;
    D_800AFEA8.count = 0;
    if (*module_loaded != 0) {
        func_801E7FD4();
        func_800320E8(D_800ADB20);
        func_8007999C();
    }
    *module_loaded = 0;
    func_8003218C(3);
    func_800A83B4();
}

/* Load a TIM's image at (x, y) and its CLUT at (clut_x, clut_y) with the
 * given size; a CLUT y of -1 or a zero size keeps the TIM's own. */
void func_80070340(u32 *tim, s16 x, s16 y, s16 clut_x, s16 clut_y, s16 clut_w, s16 clut_h) {
    TIM_IMAGE image;

    OpenTIM(tim);
    if (ReadTIM(&image) != NULL) {
        if (image.caddr != NULL) {
            if (clut_y != -1) {
                image.crect->x = clut_x;
                image.crect->y = clut_y;
            }
            if (clut_w != 0) {
                image.crect->w = clut_w;
            }
            if (clut_h != 0) {
                image.crect->h = clut_h;
            }
            LoadImage(image.crect, image.caddr);
        }
        /* Only the x placement is guarded; the image is always loaded. */
        if (image.paddr != NULL) {
            image.prect->x = x;
        }
        image.prect->y = y;
        LoadImage(image.prect, image.paddr);
    }
}

/* Start the map's own stream (file 0xb9 + 2 * map) into a four-sector ring,
 * unless one already runs. */
void func_80070488(void) {
    void *ring;

    if (D_800ADB60 == 0) {
        D_800ADB60 = 1;
        D_800ADC14 = ring = func_8002A260(4, 1);
        func_80029EB0((D_8004F34C & 0xFFF) * 2 + 0xB9, ring, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}

/* Stop the field stream and release its ring, then continue with 80078c5c. */
void func_80070508(void) {
    if (D_800ADB60 == 1) {
        func_80028A60(0);
        DrawSync(0);
        func_800320E8(D_800ADC14);
        D_800ADB60 = 0;
    }
    func_80078C5C();
}

/* Widen a short vector to 16.16 fixed point. */
void func_80070560(VECTOR *out, SVECTOR *in) {
    out->vx = in->vx << 16;
    out->vy = in->vy << 16;
    out->vz = in->vz << 16;
}

/* Identity rotation with a zero translation. */
void func_80070594(MATRIX *m) {
    SVECTOR angles;

    angles.vx = 0;
    angles.vy = 0;
    angles.vz = 0;
    func_8003F738(&angles, m);
    m->t[2] = 0;
    m->t[1] = 0;
    m->t[0] = 0;
}

/* Reset the field state for a new map: flags, counters, the screen and
 * camera defaults, the event variables from the saved game, and the view
 * matrices. */
void func_800705DC(void) {
    VECTOR unused; /* unreferenced, but part of the frame */
    SVECTOR angles;
    s32 i;

    if (D_800C268C == 0) {
        func_8028125C();
    }
    func_80071F64(0, 0, 0x140, 0xE0);
    func_80035DB0();
    D_800ADB00 = 0xFFFF;
    D_800AFE9C = 0;
    D_800AFEA0 = 0;
    D_800C2694 = 0;
    D_800C38F8 = 0;
    D_800C3900 = 0;
    D_800C3908 = 0;
    D_800B2078.unk2356 = 5;
    D_800B2078.animation_mode = 3;
    D_800B2078.unk234A = 0x40;
    D_800B2078.battle_override = 0xFF;
    D_800C3A38 = 0xFF;
    D_800C3A60 = 0;
    D_800C3A5C = 0;
    D_800B2078.unk21BC[0] = 0;
    D_800B2078.unk21BC[1] = 0;
    D_800B2078.unk21BC[2] = 0;
    D_800B2078.unk2264 = 0;
    D_800B2078.last_sound_effect = 0;
    D_800B2078.script_control[1] = 0;
    D_800B2078.script_control[0] = 0;
    D_800B2078.effect_value[5] = 0;
    D_800B2078.effect_value[4] = 0;
    D_800B2078.effect_value[3] = 0;
    D_800B2078.effect_value[2] = 0;
    D_800B2078.effect_value[1] = 0;
    D_800B2078.effect_value[0] = 0;
    D_800B2078.unk20B0[1] = 0;
    D_800B2078.unk20B0[0] = 0;
    D_800B2078.unk2268 = 0;
    D_800B2078.preserve_nonplayer_motion = 0;
    D_800B2078.piece_drift[1] = 0;
    D_800B2078.piece_drift[2] = 0;
    D_800B2078.piece_drift[0] = 0;
    D_800B2078.unk2078 = 0;
    D_800B2078.camera_floor_fixed = 0;
    D_800B2078.party_bits = 0;
    D_800B2078.clear_color[2] = 0;
    D_800B2078.clear_color[1] = 0;
    D_800B2078.clear_color[0] = 0;
    D_800B2078.party_processing_mode = 0;
    D_800B2078.forced_position = 0;
    D_800B2078.piece_drift_mode = 0;
    D_800ADB74 = 0;
    D_800ADB4C = 0;
    D_800ADB90 = 0;
    D_800ADB24 = 0;
    D_800ADB98 = 0;
    D_800ADB94 = 0;
    D_800ADB70 = 0;
    D_800ADB2C = 0;
    D_800ADB68 = 0;
    D_800ADB88 = 0;
    D_800ADBA8 = 0;
    D_800B0064 = 0;
    D_800ADB18 = 0;
    D_800ADB50 = 0;
    D_800AFE84 = 0;
    D_800AFD04 = 0;
    D_800B02C8 = 0;
    D_800ADBD4 = 0;
    D_800ADBD0 = 0;
    D_800B2078.unk22E0 = 0;
    D_800B2078.effects_kept = 0;
    D_800B14A4 = 0;
    D_800B2078.unk236C = 0;
    D_800ADB38 = 0;
    D_800ADB3C = 0;
    D_800AFD14 = 0x20;
    D_800B0048 = 2;
    D_800B2078.emitter_range = 0x3FF;
    D_800ADC18 = 4;
    D_800ADB44 = 0;
    D_800ADB02 = 0;
    D_800B2078.unk233E = 0;
    D_800B2078.jump_mode = 0;
    D_800B2078.repeat_remaining = 0;
    D_800B2078.unk2348 = 0;
    D_800B2078.followers_idle = 0;
    D_800B2078.unk2355 = 0;
    D_800ADB04 = 0;
    D_800ADB05 = 0;
    D_800B2078.unk2357 = 0;
    D_800B2078.unk2354 = 0;
    D_800ADBB4 = 0;
    D_800B2078.unk2350 = 0;
    D_800ADB54 = 0;
    D_800B2078.unk2358 = 0;
    D_800ADB84 = 0;
    D_800ADB7C = 0;
    D_800ADB8C = 0;
    D_800ADB64 = 0xFF;
    angles.vx = 0;
    angles.vy = 0;
    angles.vz = 0;
    func_8003F738(&angles, &D_800B00E8);
    for (i = 0; i < 3; i++) {
        D_800B2078.emitter_descriptor[i] = -1;
    }
    D_800AF880.scripted_scale = 0x1000;
    D_800ADC08 = 1;
    D_800B2078.unk21D4 = 0x720;
    D_800B2078.unk21A0[2] = 0x100;
    D_800B2078.unk21A0[1] = 0x100;
    D_800B2078.unk21A0[0] = 0x100;
    D_800B2078.unk21A0[5] = 0x200;
    D_800B2078.unk21A0[4] = 0x200;
    D_800B2078.unk21A0[3] = 0x200;
    D_800B2078.unk21B4 = 0x80;
    D_800ADBC4 = 0xFF;
    D_800B2078.scale = 0x1000;
    D_800B2078.sprite_angles.vx = 0;
    D_800B2078.sprite_angles.vy = 0;
    D_800B2078.sprite_angles.vz = 0;
    D_800ADB6C = -1;
    for (i = 0; i < 16; i++) {
        D_800B2078.encounter_music[i] = 0x1D;
    }
    D_800B2290 = 0x1D;
    D_800ADBEC = -1;
    D_800B2078.camera_counter = 2;
    D_800B2078.input_mask = 0xFFFF;
    D_800B2078.fog_color[2] = 0x80;
    D_800B2078.fog_color[1] = 0x80;
    D_800B2078.fog_color[0] = 0x80;
    D_800B2078.far_color[2] = 0xFF;
    D_800B2078.far_color[1] = 0xFF;
    D_800B2078.far_color[0] = 0xFF;
    D_800B2078.fog_range[0] = 0x15E0;
    D_800B2078.fog_range[1] = 0x300C;
    D_800ADB08 = 0;
    D_800ADC0C = 0;
    D_800AFEA8.count = 0;
    D_800B2078.controlled = 0;
    D_800B2078.terrain_angle = 0;
    D_800B2078.open_windows = 0;
    D_800B2078.encounter_inhibition = 0;
    D_800B2078.unk2180 = 0;
    D_800B2078.unk217C = 0;
    D_800B2078.sprite_gate = 0;
    D_800B2078.text_speed = 8;
    if (D_8004F30C == 0) {
        for (i = 0; i < 3; i++) {
            D_8005A444[i] = 0xFF;
            D_8006F990[i] = 0xFF;
        }
    }
    for (i = 0; i < 32; i++) {
        D_800B2078.unk22A0[i] = 0xFFFF;
    }
    D_800B2078.unk229C = 0;
    D_800B2078.unk2298 = 0;
    D_80050100 = 2;
    for (i = 0; i < 0x200; i++) {
        D_800C3A68[i] = D_8005A39C->vars[i];
        D_800C3A68[i + 0x200] = 0;
    }
    SetGeomScreen(0x200);
    func_80070594(&D_800AF880.previous_view);
    func_80070594(&D_800AF85C);
    func_80070594(&D_800AF880.scaled_world);
    func_80070594(&D_800AF880.world_matrix);
    D_800AF880.world_angles.vx = 0;
    D_800AF880.world_angles.vy = 0;
    D_800AF880.world_angles.vz = 0;
    D_800AF880.anchor.vx = 0;
    D_800AF880.anchor.vy = 0;
    D_800AF880.anchor.vz = 0;
    D_800AF880.scale = 0x3000;
    func_8003F738(&D_800AF880.world_angles, &D_800AF880.scaled_world);
    D_800C426C = &D_800B249C[0];
    func_8007254C();
    func_80070C84();
    func_800864B4();
    func_800A9274();
    func_800ABD18();
}

/* Reset the three slots at 800b06a4 and clear 800adb0c. */
void func_80070C84(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        D_800B06A4[i].a = 0xFF;
        D_800B06A4[i].b = 0xFF;
    }
    D_800ADB0C = 0;
}

/* Load the field from the map bundle read ahead: reset the field state, take
 * the sprite slot table, build the compass quads, decode each component
 * (palettes, images, models, events, messages, zones, collision, sprites)
 * into heap blocks, place the view, create every descriptor's model
 * instance and actor, then initialise the event layer, the camera goals and
 * the actors' facings.
 * One word pointer serves as the palette block and then walks the model
 * and collision offset tables.
 * The chained unk90 = unkA0[0] store keeps the unk90 address pseudo first
 * (it is expanded before the inner store) while storing unkA0[0] first.
 * One offset local holds the descriptor's model index and then the mesh's
 * offset in the model block (it dies twice, so the sum stays out of the
 * mesh argument's register). */
void func_80070CC8(void) {
    VECTOR unused = {0, -100, 2000, 0};
    s32 *table;
    s32 *data;
    s32 *images;
    s32 *entry;
    s32 offset;
    s32 size;
    s32 count;
    s32 i;
    s32 j;
    u16 x;
    u16 y;
    u16 *record;
    FieldInstance *instance;

    func_800705DC();
    D_800B1F78 = D_8005A4E0->slots;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            func_8007A7F4(&D_800B06BC[i * 4 + j], j, i, 0);
        }
    }
    func_8007A7F4(&D_800B06BC[16], 4, 4, 1);
    func_8007A7F4(&D_800B06BC[17], 5, 5, 1);
    func_8007A7F4(&D_800B06BC[18], 6, 6, 1);
    func_8007A7F4(&D_800B06BC[19], 7, 7, 1);
    func_8007A7F4(&D_800B06BC[20], 8, 8, 1);
    func_8007A5C4();

    size = BUNDLE_SIZE(BUNDLE_PALETTES) + 0x10;
    data = func_80031BDC(size, 1);
    func_8007008C(size, BUNDLE_COMPONENT(BUNDLE_PALETTES), data);
    entry = data;
    count = *entry;
    for (i = 0; i < count; i++) {
        entry++;
        func_800771F8((u32 *)(*entry + (s32)data));
    }

    size = BUNDLE_SIZE(BUNDLE_IMAGES) + 0x10;
    images = func_80031BDC(size, 0);
    func_8007008C(size, BUNDLE_COMPONENT(BUNDLE_IMAGES), images);
    entry = images;
    count = *entry;
    for (i = 0; i < count; i++) {
        x = D_800B1F78.slot[i].x;
        y = D_800B1F78.slot[i].y;
        entry++;
        if (D_800B1F78.slot[i].shared == 0) {
            func_80022A70((void *)(*entry + (s32)images), x, y);
        }
    }
    DrawSync(0);
    func_800320E8(data);
    func_800320E8(images);

    size = BUNDLE_SIZE(BUNDLE_MODELS) + 0x10;
    D_800AF880.components.geometry = func_80031BDC(size, 0);
    func_8007008C(size, BUNDLE_COMPONENT(BUNDLE_MODELS), D_800AF880.components.geometry);
    data = D_800AF880.components.geometry;
    for (i = 0; i < *D_800AF880.components.geometry; i++) {
        data++;
        func_8002C3E8((void *)(*data + (s32)D_800AF880.components.geometry));
    }

    func_8007008C(BUNDLE_SIZE(BUNDLE_MESSAGES) + 0x10,
                  BUNDLE_COMPONENT(BUNDLE_MESSAGES), D_800658DC);

    size = BUNDLE_SIZE(BUNDLE_EVENTS) + 0x10;
    D_800ADBF8 = func_80031BDC(size, 0);
    func_8007008C(size, BUNDLE_COMPONENT(BUNDLE_EVENTS), D_800ADBF8);
    D_800ADBFC = D_800ADBF8->count;
    D_800ADC00 = (u8 *)D_800ADBF8->entries;
    D_800ADC00 += D_800ADBFC * 64;

    size = BUNDLE_SIZE(BUNDLE_ZONES) + 0x10;
    D_800ADBF4 = func_80031BDC(size, 0);
    func_8007008C(size, BUNDLE_COMPONENT(BUNDLE_ZONES), D_800ADBF4);

    size = BUNDLE_SIZE(BUNDLE_8) + 0x10;
    D_800ADBF0 = func_80031BDC(size, 0);
    func_8007008C(size, BUNDLE_COMPONENT(BUNDLE_8), D_800ADBF0);

    size = BUNDLE_SIZE(BUNDLE_COLLISION) + 0x10;
    D_800AF880.components.collision = func_80031BDC(size, 0);
    func_8007008C(size, BUNDLE_COMPONENT(BUNDLE_COLLISION), D_800AF880.components.collision);
    D_800AF880.components.layer_count = *D_800AF880.components.collision;
    data = D_800AF880.components.collision;
    data++;
    for (i = 0; i < 4; i++) {
        D_800AF880.components.triangle_counts[i] = (u32)*data++ / sizeof(CollisionTriangle);
    }
    D_800AF880.components.collision_attributes = (Attribute *)(*data++ + (s32)D_800AF880.components.collision);
    for (i = 0; i < D_800AF880.components.layer_count; i++) {
        D_800AF880.components.collision_triangles[i] =
            (CollisionTriangle *)(*data++ + (s32)D_800AF880.components.collision);
        D_800AF880.components.collision_vertices[i] = (void *)(*data++ + (s32)D_800AF880.components.collision);
    }
    D_800AFD10 = (Attribute *)D_800AF880.components.collision_triangles[0] - D_800AF880.components.collision_attributes;

    size = BUNDLE_SIZE(BUNDLE_SPRITES) + 0x10;
    D_800AF880.components.sprites = func_80031BDC(size, 0);
    func_8007008C(size, BUNDLE_COMPONENT(BUNDLE_SPRITES), D_800AF880.components.sprites);

    D_800AF880.bounds[0] = 1;
    D_800AF880.bounds[1] = 1;
    D_800AF880.bounds[2] = 1;
    D_800AF880.bounds[3] = 1;
    func_8006FDEC(D_8005A4E0->view);

    count = D_8005A4E0->descriptor_count;
    record = D_8005A4E0->descriptors;
    D_800AF880.components.descriptor_count = count;
    table = func_80031BDC(count * sizeof(FieldDescriptor), 0);
    D_800AF880.components.descriptors = (FieldDescriptor *)table;
    size = count * (s32)(sizeof(FieldDescriptor) / sizeof(s32));
    for (i = 0; i < size; i++) {
        *table++ = 0;
    }
    for (i = 0; i < D_800AF880.components.descriptor_count; i++) {
        D_800AF880.components.descriptors[i].flags = *record++;
        D_800AF880.components.descriptors[i].rotation.vx = record[0];
        D_800AF880.components.descriptors[i].rotation.vy = record[1];
        D_800AF880.components.descriptors[i].rotation.vz = record[2];
        record += 3;
        D_800AF880.components.descriptors[i].transform.t[0] = D_800AF880.components.descriptors[i].matrix.t[0] = record[0];
        D_800AF880.components.descriptors[i].transform.t[1] = D_800AF880.components.descriptors[i].matrix.t[1] = record[1];
        D_800AF880.components.descriptors[i].transform.t[2] = D_800AF880.components.descriptors[i].matrix.t[2] = record[2];
        record += 3;
        if (!(D_800AF880.components.descriptors[i].flags & 0x40)) {
            instance = func_80031BDC(0x24, 0);
            D_800AF880.components.descriptors[i].instance = instance;
            offset = *record;
            entry = (s32 *)(offset * 4 + (s32)D_800AF880.components.geometry);
            offset = entry[1] + (s32)D_800AF880.components.geometry;
            instance->mesh = (FieldMesh *)(offset + 0x10);
            func_8002CB54(instance->mesh, &instance->packets[0], &instance->packets[1]);
            func_8002C8CC(instance->mesh, instance->packets[0], (D_800AF880.components.descriptors[i].flags & 0xC) >> 2);
            memcpy(instance->packets[1], instance->packets[0], instance->mesh->size);
            if (D_800AF880.components.descriptors[i].flags & 0x2000) {
                func_80032498(3, 0);
                instance->anims = func_800303C8(instance->mesh, 0);
                func_80032498(8, 0);
            }
            func_8002C644(instance->mesh);
            func_80080F44(i);
        } else {
            D_800AF880.components.descriptors[i].flags |= 0x20;
            D_800AF880.components.descriptors[i].rotation.vx = 0;
            D_800AF880.components.descriptors[i].rotation.vy = 0;
            D_800AF880.components.descriptors[i].rotation.vz = 0;
            func_80080F44(i);
        }
        record++;
    }
    if (D_800C268C == 0) {
        func_802812A4();
    }
    func_8007DECC();
    func_80071A64();
    func_800320B8(D_8005A4E0);
    func_800320E8(D_8005A4E0);
    func_80032498(5, 0);
    func_80024F64(0x3C00, 0);
    func_8001C944();
    func_80032498(8, 0);
    func_80077844((MATRIX *)D_800B2078.unk223C, 0x800, 0, 0, 0x800, 0, 0, 0x800, 0, 0);
    func_80077844((MATRIX *)D_800B2078.unk221C, 0x1F8, -0xFC1, -0x1F8, 0, 0, 0, 0, 0, 0);
    D_800B2078.unk225C[2] = 0x1E;
    D_800B2078.unk225C[1] = 0x1E;
    D_800B2078.unk225C[0] = 0x1E;
    D_800B0080.unk80[2] = 0x140;
    D_800B0080.unk80[7] = 0;
    D_800B0080.unkA8[1] = 0;
    D_800B0080.unkA8[0] = 0;
    D_800B0080.unkA4[2] = 0;
    D_800B0080.unkA4[1] = 0;
    D_800B0080.unkA4[0] = 0;
    D_800B0080.unkA0[2] = 0;
    D_800B0080.unkA0[1] = 0;
    D_800B0080.unk90 = D_800B0080.unkA0[0] = 0;
    D_800B0080.unk98 = 0x1000;
    D_800B0080.unkB0 = 0;
    D_800B0080.unkAE = 0;
    D_800B0080.unkAC = 0;
    D_800B0080.unk80[6] = 0;
    D_800B0080.unk80[5] = 0;
    D_800B0080.unk80[4] = 0;
    D_800B0080.unk80[3] = 0;
    D_800B0080.unk80[1] = 0;
    D_800B0080.unk80[0] = 0;
    D_800B0080.enabled = 0;
    D_800B0080.unk94 = 0;
    D_800B0080.unkA8[2] = 0x20;
    D_800ADB1C = 0;
    func_800A28D4();
    D_800ADB1C = 1;
    func_8003F738(&D_800B2078.sprite_angles, &D_800AFC30);
    D_800AFC30.t[2] = 0;
    D_800AFC30.t[1] = 0;
    D_800AFC30.t[0] = 0;
    if (D_800B0080.enabled != 0) {
        D_800B007C = func_8002709C(D_800B0080.unk80[0], D_800B0080.unk80[1], D_800B0080.unk80[2], D_800B0080.unk80[3],
                                   D_800B0080.unk80[4], D_800B0080.unk80[5], D_800B0080.unk80[6], D_800B0080.unk80[7],
                                   &D_800B0080.unk90, D_800B0080.unkA0, D_800B0080.unkAC, D_800B0080.unkAE,
                                   D_800B0080.unkB0);
    }
    func_80032498(8, 0);
    D_800AF880.target_goal.vx = D_800AF880.components.descriptors[D_800B2078.unk233E].matrix.t[0] << 16;
    D_800AF880.target_goal.vy = D_800AF880.components.descriptors[D_800B2078.unk233E].matrix.t[1] << 16;
    D_800AF880.target_goal.vz = D_800AF880.components.descriptors[D_800B2078.unk233E].matrix.t[2] << 16;
    for (i = 0; i < D_800AF880.components.descriptor_count; i++) {
        func_8003F738(&D_800AF880.components.descriptors[i].rotation, &D_800AF880.components.descriptors[i].matrix);
        D_800AF880.components.descriptors[i].transform = D_800AF880.components.descriptors[i].matrix;
    }
    func_80077C60();
    func_800A2714();
    D_8004F334 = -1;
    D_8004F330 = -1;
    func_80073E38();
    func_80077268();
    if (D_800B0080.enabled == 0) {
        D_800ADB4C = func_8007469C();
    } else {
        D_800ADB4C = 1;
    }
    for (i = 0; i < D_800ADBFC; i++) {
        if (D_800AF880.components.descriptors[i].flags & 0x40) {
            if (!(D_800AF880.components.descriptors[i].actor->layer_flags & 0x01000000)) {
                func_800223B0(D_800AF880.components.descriptors[i].model,
                              D_800AF880.view_angle + D_800AF880.components.descriptors[i].actor->unk108);
            } else {
                func_80021FE0(D_800AF880.components.descriptors[i].model,
                              D_800AF880.components.descriptors[i].actor->unk108);
            }
        }
    }
}

/* Initialise both fade channels' primitives. */
void func_80071A64(void) {
    func_8007D93C(0);
    func_8007D93C(1);
}

/* Advance one fade channel a step; a finished channel whose levels reached
 * zero turns off unless a fade-out is still in progress. */
void func_80071A8C(s32 channel) {
    if (D_800B2078.fades[channel].active != 0) {
        if (D_800B2078.fades[channel].steps <= 0) {
            D_800B2078.fades[channel].steps = 0;
            if (D_800ADC08 != 1 && D_800B2078.fades[channel].level[0] == 0 &&
                D_800B2078.fades[channel].level[1] == 0 && D_800B2078.fades[channel].level[2] == 0) {
                D_800B2078.fades[channel].active = 0;
            }
        } else {
            D_800B2078.fades[channel].level[0] = D_800B2078.fades[channel].level[0] + D_800B2078.fades[channel].step[0];
            if (D_800B2078.fades[channel].level[0] >> 8 >= 0x100) {
                D_800B2078.fades[channel].level[0] = 0xFF00;
            }
            if (D_800B2078.fades[channel].level[0] < 0) {
                D_800B2078.fades[channel].level[0] = 0;
            }
            D_800B2078.fades[channel].level[1] = D_800B2078.fades[channel].level[1] + D_800B2078.fades[channel].step[1];
            if (D_800B2078.fades[channel].level[1] >> 8 >= 0x100) {
                D_800B2078.fades[channel].level[1] = 0xFF00;
            }
            if (D_800B2078.fades[channel].level[1] < 0) {
                D_800B2078.fades[channel].level[1] = 0;
            }
            D_800B2078.fades[channel].level[2] = D_800B2078.fades[channel].level[2] + D_800B2078.fades[channel].step[2];
            if (D_800B2078.fades[channel].level[2] >> 8 >= 0x100) {
                D_800B2078.fades[channel].level[2] = 0xFF00;
            }
            if (D_800B2078.fades[channel].level[2] < 0) {
                D_800B2078.fades[channel].level[2] = 0;
            }
            D_800B2078.fades[channel].steps = D_800B2078.fades[channel].steps - 1;
        }
    }
}

/* Step both fade channels while fading, then draw them into `ot`. */
void func_80071CB4(void *ot, s32 buffer) {
    if (D_800ADC04 == 2) {
        func_80071A8C(0);
        func_80071A8C(1);
    }
    func_8007DA44(ot, D_800ADB08);
}

/* Start a fade on `channel` towards (red, green, blue) over `steps` frames. */
void func_80071D08(s32 channel, s32 steps, s32 red, s32 green, s32 blue, s32 abr) {
    s32 red_step = ((red << 8) - D_800B2078.fades[channel].level[0]) / steps;
    s32 green_step = ((green << 8) - D_800B2078.fades[channel].level[1]) / steps;
    s32 blue_step = ((blue << 8) - D_800B2078.fades[channel].level[2]) / steps;

    D_800B2078.fades[channel].steps = steps;
    D_800B2078.fades[channel].active = 1;
    D_800B2078.fades[channel].abr = abr;
    D_800B2078.fades[channel].step[0] = red_step;
    D_800B2078.fades[channel].step[1] = green_step;
    D_800B2078.fades[channel].step[2] = blue_step;
}

/* Fade channel 0 out to white over `steps` frames, once. */
void func_80071DCC(s32 steps) {
    s32 rate;

    if (D_800ADC08 != 1) {
        D_800ADC08 = 1;
        if (D_800ADC04 == 2) {
            rate = 0xFF00 / steps;
            D_800B2078.fades[0].level[0] = D_800B2078.fades[0].level[1] = D_800B2078.fades[0].level[2] = 0;
            D_800B2078.fades[0].steps = steps;
            D_800B2078.fades[0].active = 1;
            D_800B2078.fades[0].abr = 2;
            D_800B2078.fades[0].step[0] = D_800B2078.fades[0].step[1] = D_800B2078.fades[0].step[2] = rate;
        }
    }
}

/* Fade channel 0 back in from full over `steps` frames, once. */
void func_80071E58(s32 steps) {
    s32 rate;

    if (D_800ADC08 != 0) {
        D_800ADC08 = 0;
        if (D_800ADC04 == 2) {
            rate = -0x10000 / steps;
            D_800B2078.fades[0].level[0] = D_800B2078.fades[0].level[1] = D_800B2078.fades[0].level[2] = 0xFF00;
            D_800B2078.fades[0].active = 1;
            D_800B2078.fades[0].steps = steps;
            D_800B2078.fades[0].abr = 2;
            D_800B2078.fades[0].step[0] = D_800B2078.fades[0].step[1] = D_800B2078.fades[0].step[2] = rate;
        }
    }
}

/* Pointer setup: pad buffers, divisors 3 and 4, bounds, both ports' starts. */
void func_80071EE8(void) {
    func_8007AD8C(D_800625FC[0], D_800625FC[1]);
    func_8007AE14(3, 4);
    func_8007ADA4(0, 0x140, 0, 0xE0);
    func_8007AE2C(0, 0x50, 0x64);
    func_8007AE2C(1, 0xFA, 0x64);
    func_8007ADA4(0, 0x12C, 0xA, 0xDC);
}

/* Set both draw buffers' clip areas; the second sits 0x100 lines lower. */
void func_80071F64(s32 x, s32 y, s32 w, s32 h) {
    D_800B249C[0].draw.clip.x = x;
    D_800B249C[0].draw.clip.y = y;
    D_800B249C[0].draw.clip.w = w;
    D_800B249C[0].draw.clip.h = h;
    D_800B249C[1].draw.clip.x = x;
    D_800B249C[1].draw.clip.y = y + 0x100;
    D_800B249C[1].draw.clip.w = w;
    D_800B249C[1].draw.clip.h = h;
}

/* Display setup: geometry defaults and offset, both draw blocks'
 * environments, clip areas and screens, black backgrounds, then show the
 * second block and set the renderer's limits. */
void func_80071FB0(void) {
    D_80059198 = 1;
    DrawSync(0);
    VSync(0);
    InitGeom();
    SetGeomOffset(0xA0, 0x70);
    SetDefDrawEnv(&D_800B249C[0].draw, 0, 0, 0x140, 0xE0);
    SetDefDrawEnv(&D_800B249C[1].draw, 0, 0x100, 0x140, 0xE0);
    SetDefDrawEnv(&D_800B249C[0].draw2, 0, 0, 0x140, 0xE0);
    SetDefDrawEnv(&D_800B249C[1].draw2, 0, 0x100, 0x140, 0xE0);
    SetDefDispEnv(&D_800B249C[0].disp, 0, 0x100, 0x140, 0xE0);
    SetDefDispEnv(&D_800B249C[1].disp, 0, 0, 0x140, 0xE0);
    func_80071F64(0, 0, 0x140, 0xE0);
    func_80086D8C();
    D_800B249C[0].draw.r0 = 0;
    D_800B249C[0].draw.g0 = 0;
    D_800B249C[0].draw.b0 = 0;
    D_800B249C[1].draw.r0 = 0;
    D_800B249C[1].draw.g0 = 0;
    D_800B249C[1].draw.b0 = 0;
    D_800B249C[0].draw.dtd = 1;
    D_800B249C[1].draw.dtd = 1;
    VSync(0);
    PutDispEnv(&D_800B249C[1].disp);
    PutDrawEnv(&D_800B249C[1].draw);
    func_8002DFF0(0x140, 0xF0);
}

/* Clear a matrix's translation. */
void func_80072140(MATRIX *m) {
    m->t[2] = 0;
    m->t[1] = 0;
    m->t[0] = 0;
}

/* Compose the view (orbit under the previous view), the world matrices, and
 * leave the scaled world matrix loaded for drawing. */
void func_80072150(void) {
    VECTOR scale;
    MATRIX unused;
    MATRIX composed;
    s32 flag;

    func_8003F738(&D_800AF880.orbit_angles, &D_800AF880.orbit);
    func_80072140(&D_800AF880.orbit);
    CompMatrix(&D_800AF880.orbit, &D_800AF880.previous_view, &composed);
    func_80074038(&D_800AF880.previous_view, &composed);
    func_8003F738(&D_800AF880.world_angles, &D_800AF880.world_matrix);
    func_80072140(&D_800AF880.world_matrix);
    func_8003F738(&D_800AF880.world_angles, &D_800AF880.scaled_world);
    MulMatrix2(&D_800AF880.previous_view, &D_800AF880.scaled_world);
    SetRotMatrix(&D_800AF880.previous_view);
    SetTransMatrix(&D_800AF880.previous_view);
    func_8004A6DC(&D_800AF880.anchor, D_800AF880.scaled_world.t, &flag);
    scale.vx = D_800AF880.scale;
    scale.vy = D_800AF880.scale;
    scale.vz = D_800AF880.scale;
    ScaleMatrix(&D_800AF880.scaled_world, &scale);
    SetRotMatrix(&D_800AF880.scaled_world);
    SetTransMatrix(&D_800AF880.scaled_world);
}

/* Rebuild a descriptor's matrix from its rotation, scaled by its actor. */
void func_80072254(s32 index) {
    VECTOR scale;

    scale.vx = D_800AF880.components.descriptors[index].actor->scale[0];
    scale.vy = D_800AF880.components.descriptors[index].actor->scale[1];
    scale.vz = D_800AF880.components.descriptors[index].actor->scale[2];
    func_8003F738(&D_800AF880.components.descriptors[index].rotation, &D_800AF880.components.descriptors[index].matrix);
    ScaleMatrix(&D_800AF880.components.descriptors[index].matrix, &scale);
}

/* Compose the view and reload the scaled world matrix; 802815b0 runs unless
 * 800c268c is set. */
void func_800722F4(void) {
    func_80072150();
    SetRotMatrix(&D_800AF880.scaled_world);
    SetTransMatrix(&D_800AF880.scaled_world);
    if (D_800C268C == 0) {
        func_802815B0();
    }
}

/* Count the blocked octants from `start` upward; zero when all are blocked. */
s32 func_8007234C(s32 mask, s32 start) {
    s32 i;
    s32 count;

    for (i = 0, count = 0; i < 8; i++, count++) {
        if (!(mask & D_800ADC1C[start++ & 7])) {
            return count;
        }
    }
    return 0;
}

/* Count the blocked octants from `start` downward; zero when all are blocked. */
s32 func_80072398(s32 mask, s32 start) {
    s32 i;
    s32 count;

    for (i = 0, count = 0; i < 8; i++, count++) {
        if (!(mask & D_800ADC1C[start-- & 7])) {
            return count;
        }
    }
    return 0;
}

/* The intersection of the lines through segments `a` and `b` (X/Z points);
 * `b`'s start when they are parallel. */
void func_800723E4(DVECTOR *a, DVECTOR *b, DVECTOR *out) {
    VECTOR ua;
    VECTOR ub;
    VECTOR d;
    s32 cross;
    s32 t;

    d.vx = a[1].vx - a[0].vx;
    d.vy = 0;
    d.vz = a[1].vy - a[0].vy;
    VectorNormal(&d, &ua);
    d.vx = b[1].vx - b[0].vx;
    d.vy = 0;
    d.vz = b[1].vy - b[0].vy;
    VectorNormal(&d, &ub);
    cross = (ub.vx * ua.vz - ub.vz * ua.vx) >> 12;
    if (cross == 0) {
        t = 0;
    } else {
        t = ((b[0].vy - a[0].vy) * ua.vx - (b[0].vx - a[0].vx) * ua.vz) / cross;
    }
    out->vx = b[0].vx + ((t * ub.vx) >> 12);
    out->vy = b[0].vy + ((t * ub.vz) >> 12);
}

/* Reset the orbit camera. Its do-while scope is what keeps cse2 from folding
 * the 800af984-relative addresses after the call back into absolute ones. */
#define CAMERA_RESET_ORBIT() do { func_80070594(&D_800AF880.orbit); } while (0)

/* The camera's initial state. The eye pointer gives the original's
 * $s0-relative eye.vx with absolute eye.vy/vz; the up vectors' y is loaded
 * once ahead of the eye stores. */
void func_8007254C(void) {
    VECTOR *eye;
    s32 up_y;

    D_800AF880.target_a = 8;
    D_800AF880.target_b = 8;
    D_800AF880.heading_velocity = 0x400000;
    D_800AF880.heading_high = 0x08000000;
    D_800AF880.heading_angles.vy = 0x800;
    D_800AF880.shake_amplitude[2] = 0;
    D_800AF880.shake_amplitude[1] = 0;
    D_800AF880.shake_amplitude[0] = 0;
    D_800AF880.shake_step[2] = 0;
    D_800AF880.shake_step[1] = 0;
    D_800AF880.shake_step[0] = 0;
    D_800AF880.shake = 0;
    D_800AF880.shake_stop = 0;
    D_800AF880.flags = 0;
    D_800AF880.view_angle = 0;
    D_800AF880.angle = 0;
    D_800AF880.heading_blocks[0] = 0;
    D_800AF880.heading_blocks[1] = 0;
    D_800AF880.heading_steps = 0;
    D_800AF880.heading_angles.vx = 0;
    D_800AF880.heading_angles.vz = 0;
    D_800AF880.heading = 0x800;
    D_800AF880.orbit_angles.vx = 0;
    D_800AF880.orbit_angles.vy = 0;
    D_800AF880.orbit_angles.vz = 0;
    CAMERA_RESET_ORBIT();
    eye = &D_800AF880.eye;
    up_y = 0x10000000;
    eye->vx = 0;
    eye->vy = 0;
    eye->vz = 0;
    D_800AF880.up.vy = up_y;
    D_800AF880.unk050.vy = up_y;
    D_800AF880.elevation = 0x1E;
    D_800AF880.projection = 0x200;
    D_800AF880.target.vx = 0;
    D_800AF880.target.vy = 0;
    D_800AF880.target.vz = 0;
    D_800AF880.up.vx = 0;
    D_800AF880.up.vz = 0;
    D_800AF880.shake_offset.vx = 0;
    D_800AF880.shake_offset.vy = 0;
    D_800AF880.shake_offset.vz = 0;
    D_800AF880.eye_goal.vx = 0;
    D_800AF880.eye_goal.vy = 0;
    D_800AF880.eye_goal.vz = 0;
    D_800AF880.target_goal.vx = 0;
    D_800AF880.target_goal.vy = 0;
    D_800AF880.target_goal.vz = 0;
    D_800AF880.unk050.vx = 0;
    D_800AF880.unk050.vz = 0;
    D_800AF880.elevation_steps = 0;
    D_800AF880.projection_steps = 0;
    D_800AF880.steps = 0;
    D_800AF880.distance = 0x1000;
    D_800AF880.mode = 0;
    D_800AF880.scripted = 0;
    D_800AF880.target_steps = 0;
    D_800AF880.eye_steps = 0;
}

/* Turn the camera heading by an octant when the current one is blocked or
 * a shoulder button asks for it, then step the heading toward its goal. */
void func_800726E8(void) {
    s32 up;

    if (D_800AF880.heading_blocks[0] != 0xFF && D_800AF880.heading_blocks[1] != 0xFF) {
        if (D_800AF880.heading_steps == 0) {
            if (D_800AF880.heading_blocks[0] & D_800ADC1C[(D_800AF880.heading_angles.vy & 0xFFF) >> 9]) {
                if (D_800AF880.heading_velocity != -0x400000 && D_800AF880.heading_velocity != 0x400000) {
                    D_800AF880.heading_velocity = 0x400000;
                    D_800AF880.heading += 0x200;
                }
                D_800AF880.heading_steps = 8;
            }
            if (D_800AF880.heading_blocks[1] & D_800ADC1C[(D_800AF880.heading_angles.vy & 0xFFF) >> 9]) {
                up = func_8007234C(D_800AF880.heading_blocks[1], (D_800AF880.heading_angles.vy & 0xFFF) >> 9);
                if (func_80072398(D_800AF880.heading_blocks[1], (D_800AF880.heading_angles.vy & 0xFFF) >> 9) < up) {
                    D_800AF880.heading_velocity = -0x400000;
                    D_800AF880.heading -= 0x200;
                } else {
                    D_800AF880.heading_velocity = 0x400000;
                    D_800AF880.heading += 0x200;
                }
                D_800AF880.heading_steps = 8;
            }
        }
        if ((D_800AFE9C & 4) && !(D_800AF880.flags & 0x8000) && D_800AF880.heading_steps == 0 &&
            !(D_800AF880.heading_blocks[1] & D_800ADC1C[((D_800AF880.heading_angles.vy - 0x200) & 0xFFF) >> 9])) {
            D_800AF880.heading_velocity = -0x400000;
            D_800AF880.heading_steps = 8;
            D_800AF880.heading -= 0x200;
        }
        if ((D_800AFE9C & 8) && !(D_800AF880.flags & 0x8000) && D_800AF880.heading_steps == 0 &&
            !(D_800AF880.heading_blocks[1] & D_800ADC1C[((D_800AF880.heading_angles.vy + 0x200) & 0xFFF) >> 9])) {
            D_800AF880.heading_velocity = 0x400000;
            D_800AF880.heading_steps = 8;
            D_800AF880.heading += 0x200;
        }
    }
    if (D_800AF880.heading_steps != 0) {
        D_800AF880.heading_high += D_800AF880.heading_velocity;
        D_800AF880.heading_angles.vy = D_800AF880.heading_high >> 16;
        if (--D_800AF880.heading_steps == 0) {
            D_800AF880.heading_angles.vy = D_800AF880.heading;
        }
    } else {
        D_800AF880.heading_angles.vy = D_800AF880.heading;
    }
    if (D_800C268C == 0) {
        func_80284EA4();
    }
}

/* Place the camera's target goal on the followed position (clamped to the
 * walkable edge when the position leaves the floor mesh) and its eye goal
 * at the elevation and distance around it; then step the distance and
 * elevation interpolations. Declared int but returns nothing. */
s32 func_80072A38(VECTOR *position, s32 floor) {
    SVECTOR edge[2];
    DVECTOR line[2];
    DVECTOR segment[2];
    DVECTOR hit;
    VECTOR point;
    MATRIX unused; /* unreferenced, but part of the frame */

    point.vx = position->vx;
    point.vy = 0;
    point.vz = position->vz;
    if (func_8007CD80(&point, edge, segment) == -1) {
        line[0].vx = edge[0].vx;
        line[0].vy = edge[0].vz;
        line[1].vx = edge[1].vx;
        line[1].vy = edge[1].vz;
        func_800723E4(line, segment, &hit);
        D_800AF880.target_goal.vx = hit.vx << 16;
        D_800AF880.target_goal.vz = hit.vy << 16;
        if (D_800B2078.camera_floor_fixed == 0) {
            if (D_800ADBA8 == 0) {
                D_800AF880.target_goal.vy = floor << 16;
                D_800ADBA8 = 1;
            }
        } else {
            D_800AF880.target_goal.vy = position->vy - 0x200000;
        }
    } else {
        D_800AF880.target_goal.vx = position->vx;
        D_800ADBA8 = 0;
        D_800AF880.target_goal.vy = position->vy;
        D_800AF880.target_goal.vz = position->vz;
        D_800AF880.target_goal.vy -= 0x200000;
    }
    D_800AF880.eye_goal.vy =
        ((-((func_8003F8CC((((s16)D_800AF880.elevation * 0x5B) >> 3) + 0xC00) * D_800AF880.projection) << 5)) >> 16) *
            D_800AF880.distance * 16 +
        D_800AF880.target_goal.vy;
    D_800AF880.eye_goal.vz =
        (((func_8003F8B0((((s16)D_800AF880.elevation * 0x5B) >> 3) + 0xC00) * D_800AF880.projection) << 5) >> 16) *
            D_800AF880.distance * 16 +
        D_800AF880.target_goal.vz;
    D_800AF880.eye_goal.vx = D_800AF880.target_goal.vx;
    func_80073684(&D_800AF880.eye_goal, &D_800AF880.target_goal);
    if (D_800AF880.flags & 1) {
        if (D_800AF880.steps != 0) {
            D_800AF880.start += D_800AF880.step;
            D_800AF880.distance = D_800AF880.start >> 16;
        }
        if (--D_800AF880.steps == 0) {
            D_800AF880.flags &= 0xFFFE;
        }
    }
    if (D_800AF880.flags & 8) {
        D_800AF880.elevation_value += D_800AF880.elevation_step;
        D_800AF880.elevation = D_800AF880.elevation_value >> 16;
        if (--D_800AF880.elevation_steps == 0) {
            D_800AF880.flags &= 0xFFF7;
        }
    }
}

/* Step the projection interpolation, move the eye and target a fraction of
 * the way toward their goals (skipping axes already within the follow
 * divisor), and draw a random camera shake offset. */
void func_80072D74(void) {
    s32 target_threshold;
    s32 eye_threshold;
    s32 difference;
    s32 part;

    if (D_800AF880.flags & 0x10) {
        if (D_800AF880.projection_steps != 0) {
            D_800AF880.projection_value += D_800AF880.projection_step;
            D_800AF880.projection = D_800AF880.projection_value >> 16;
        }
        if (--D_800AF880.projection_steps < 0) {
            D_800AF880.flags &= 0xFFEF;
            D_800AF880.projection_steps = 0;
        }
    }
    if (D_800B2078.camera_counter != 0) {
        D_800AF880.target_a = 1;
        D_800AF880.target_b = 1;
        D_800B2078.camera_counter--;
    }
    target_threshold = D_800AF880.target_a * D_800AF880.target_a;
    eye_threshold = D_800AF880.target_b * D_800AF880.target_b;

    if ((D_800AF880.eye.vx >> 16) != (D_800AF880.eye_goal.vx >> 16)) {
        difference = D_800AF880.eye_goal.vx - D_800AF880.eye.vx;
        part = difference >> 16;
        if (part * part >= eye_threshold) {
            D_800AF880.eye.vx += difference / D_800AF880.target_b;
        }
    }
    if ((D_800AF880.eye.vz >> 16) != (D_800AF880.eye_goal.vz >> 16)) {
        difference = D_800AF880.eye_goal.vz - D_800AF880.eye.vz;
        part = difference >> 16;
        if (part * part >= eye_threshold) {
            D_800AF880.eye.vz += difference / D_800AF880.target_b;
        }
    }
    if ((D_800AF880.eye.vy >> 16) != (D_800AF880.eye_goal.vy >> 16)) {
        difference = D_800AF880.eye_goal.vy - D_800AF880.eye.vy;
        part = difference >> 16;
        if (part * part >= eye_threshold) {
            D_800AF880.eye.vy += difference / D_800AF880.target_b;
        }
    }
    if ((D_800AF880.target.vx >> 16) != (D_800AF880.target_goal.vx >> 16)) {
        difference = D_800AF880.target_goal.vx - D_800AF880.target.vx;
        part = difference >> 16;
        if (part * part >= target_threshold) {
            D_800AF880.target.vx += difference / D_800AF880.target_a;
        }
    }
    if ((D_800AF880.target.vz >> 16) != (D_800AF880.target_goal.vz >> 16)) {
        difference = D_800AF880.target_goal.vz - D_800AF880.target.vz;
        part = difference >> 16;
        if (part * part >= target_threshold) {
            D_800AF880.target.vz += difference / D_800AF880.target_a;
        }
    }
    if ((D_800AF880.target.vy >> 16) != (D_800AF880.target_goal.vy >> 16)) {
        difference = D_800AF880.target_goal.vy - D_800AF880.target.vy;
        part = difference >> 16;
        if (part * part >= target_threshold) {
            D_800AF880.target.vy += difference / D_800AF880.target_a;
        }
    }

    D_800AF880.shake_offset.vx = 0;
    D_800AF880.shake_offset.vy = 0;
    D_800AF880.shake_offset.vz = 0;
    if (D_800AF880.shake != 0) {
        if (D_800AF880.shake_time != 0) {
            D_800AF880.shake_amplitude[0] += D_800AF880.shake_step[0];
            D_800AF880.shake_amplitude[1] += D_800AF880.shake_step[1];
            D_800AF880.shake_amplitude[2] += D_800AF880.shake_step[2];
        } else if (D_800AF880.shake_stop != 0) {
            D_800AF880.shake_amplitude[2] = 0;
            D_800AF880.shake_amplitude[1] = 0;
            D_800AF880.shake_amplitude[0] = 0;
            D_800AF880.shake = 0;
            D_800AF880.shake_stop = 0;
        }
        D_800AF880.shake_offset.vx = rand() * WHOLE(D_800AF880.shake_amplitude[0]);
        D_800AF880.shake_offset.vy = rand() * WHOLE(D_800AF880.shake_amplitude[1]);
        D_800AF880.shake_offset.vz = rand() * WHOLE(D_800AF880.shake_amplitude[2]);
        if (D_800AF880.shake_offset.vx < 0) {
            D_800AF880.shake_offset.vx = 0;
            D_800AF880.shake_amplitude[0] = 0;
        }
        if (D_800AF880.shake_offset.vy < 0) {
            D_800AF880.shake_offset.vy = 0;
            D_800AF880.shake_amplitude[1] = 0;
        }
        if (D_800AF880.shake_offset.vz < 0) {
            D_800AF880.shake_offset.vz = 0;
            D_800AF880.shake_amplitude[2] = 0;
        }
        if (D_800AF880.shake_time > 0) {
            D_800AF880.shake_time--;
        }
    }
}

/* Per-frame camera update: in mode 1 follow the scripted target and eye
 * interpolations; in modes 0 and 2 follow the controlled actor (mode 2
 * returns to 0 once both goals are reached or after 64 frames), keeping
 * the eye above the floor; then move the camera toward its goals. */
void func_80073230(void) {
    VECTOR position;
    VECTOR normal;
    SVECTOR floor;
    s32 target_distance;
    s32 eye_distance;

    switch (D_800AF880.mode) {
    case 2:
        D_800ADBAC = 0;
        if (++D_800ADBB0 >= 0x41) {
            D_800AF880.mode = 0;
        }
        goto follow;
    case 0:
        D_800ADBB0 = 0;
        if (!(D_800ADBAC & 3)) {
            if (D_800AF880.target_a < 9) {
                D_800AF880.target_a = 8;
            } else {
                D_800AF880.target_a -= 2;
            }
            if (D_800AF880.target_b < 9) {
                D_800AF880.target_b = 8;
            } else {
                D_800AF880.target_b -= 2;
            }
        }
        D_800ADBAC++;
    follow:
        func_800726E8();
        position.vx = D_800AF880.components.descriptors[D_800B2078.unk233E].actor->position[0];
        position.vy = D_800AF880.components.descriptors[D_800B2078.unk233E].actor->position[1];
        position.vz = D_800AF880.components.descriptors[D_800B2078.unk233E].actor->position[2];
        func_80072A38(&position, D_800AF880.components.descriptors[D_800B2078.unk233E].actor->unk72);
        if (!(D_800AF880.flags & 0x4000)) {
            func_8007B1C4(WHOLE(D_800AF880.eye_goal.vx), WHOLE(D_800AF880.eye_goal.vz),
                          D_800AF880.components.layer_count - 1, &floor, &normal);
            if (WHOLE(D_800AF880.eye_goal.vy) > floor.vy) {
                D_800AF880.eye_goal.vy = floor.vy << 16;
            }
        }
        if (D_800AF880.mode == 2) {
            target_distance = func_80099A4C(WHOLE(D_800AF880.target_goal.vx) - WHOLE(D_800AF880.target.vx),
                                            WHOLE(D_800AF880.target_goal.vz) - WHOLE(D_800AF880.target.vz));
            eye_distance = func_80099A4C(WHOLE(D_800AF880.eye_goal.vx) - WHOLE(D_800AF880.eye.vx),
                                         WHOLE(D_800AF880.eye_goal.vz) - WHOLE(D_800AF880.eye.vz));
            if (target_distance < 0x80 && eye_distance < 0x80) {
                D_800AF880.mode = 0;
            }
        }
        break;
    case 1:
        D_800ADBAC = 0;
        D_800ADBB0 = 0;
        if (D_800AF880.scripted & 1) {
            if (D_800AF880.target_steps != 0) {
                D_800AF880.scripted_target.vx += D_800AF880.target_step.vx;
                D_800AF880.scripted_target.vy += D_800AF880.target_step.vy;
                D_800AF880.scripted_target.vz += D_800AF880.target_step.vz;
            }
            if (--D_800AF880.target_steps == 0) {
                D_800AF880.scripted &= 0xFFFE;
            }
            D_800AF880.target_goal.vx = D_800AF880.scripted_target.vx;
            D_800AF880.target_goal.vy = D_800AF880.scripted_target.vy;
            D_800AF880.target_goal.vz = D_800AF880.scripted_target.vz;
        }
        if (D_800AF880.scripted & 2) {
            if (D_800AF880.eye_steps != 0) {
                D_800AF880.scripted_eye[0] += D_800AF880.eye_step[0];
                D_800AF880.scripted_eye[1] += D_800AF880.eye_step[1];
                D_800AF880.scripted_eye[2] += D_800AF880.eye_step[2];
            }
            if (--D_800AF880.eye_steps == 0) {
                D_800AF880.scripted &= 0xFFFD;
            }
            D_800AF880.eye_goal.vx = D_800AF880.scripted_eye[0];
            D_800AF880.eye_goal.vy = D_800AF880.scripted_eye[1];
            D_800AF880.eye_goal.vz = D_800AF880.scripted_eye[2];
        }
        break;
    }
    func_80072D74();
    D_800AF880.heading_angles.vy &= 0xFFF;
}

/* Rotate `point` in X/Z about `center` by the camera heading angles. */
void func_80073684(VECTOR *point, VECTOR *center) {
    MATRIX m;
    VECTOR offset;
    VECTOR rotated;

    PushMatrix();
    func_8003F738(&D_800AF880.heading_angles, &m);
    offset.vx = center->vx - point->vx;
    offset.vy = center->vy - point->vy;
    offset.vz = center->vz - point->vz;
    ApplyMatrixLV(&m, &offset, &rotated);
    point->vx = rotated.vx + center->vx;
    point->vz = rotated.vz + center->vz;
    PopMatrix();
}

/* Truncate a 16.16 vector to its integer parts. */
void func_80073734(VECTOR *v) {
    v->vx = ((s16 *)&v->vx)[1];
    v->vy = ((s16 *)&v->vy)[1];
    v->vz = ((s16 *)&v->vz)[1];
}

/* Build a look-at view matrix from 16.16 eye, target and up vectors: the
 * rows are the side, up and forward axes, the translation the rotated
 * eye (scaled by 3) negated. */
void func_80073750(MATRIX *view, VECTOR *eye, VECTOR *target, VECTOR *up) {
    VECTOR v;
    VECTOR forward;
    VECTOR side;
    VECTOR y;
    SVECTOR position;

    v.vx = (target->vx - eye->vx) >> 16;
    v.vy = (target->vy - eye->vy) >> 16;
    v.vz = (target->vz - eye->vz) >> 16;
    y.vx = up->vx;
    y.vy = up->vy;
    y.vz = up->vz;
    y.vx >>= 16;
    y.vy >>= 16;
    y.vz >>= 16;
    VectorNormal(&v, &forward);
    func_8004A480(&y, &forward, &v);
    VectorNormal(&v, &side);
    func_8004A480(&forward, &side, &v);
    VectorNormal(&v, &y);
    view->m[0][0] = side.vx;
    view->m[0][1] = side.vy;
    view->m[0][2] = side.vz;
    view->m[1][0] = y.vx;
    view->m[1][1] = y.vy;
    view->m[1][2] = y.vz;
    view->m[2][0] = forward.vx;
    view->m[2][1] = forward.vy;
    view->m[2][2] = forward.vz;
    position.vx = WHOLE(eye->vx) * 3;
    position.vy = WHOLE(eye->vy) * 3;
    position.vz = WHOLE(eye->vz) * 3;
    ApplyMatrix(view, &position, &v);
    view->t[0] = -v.vx;
    view->t[1] = -v.vy;
    view->t[2] = -v.vz;
}

/* Turn angle `angle` towards `goal` by `step` the short way round, stopping
 * at the goal; 12-bit angles. */
s32 func_80073930(s32 angle, s32 goal, s32 step) {
    if (((angle - goal) & 0xFFF) < 0x800) {
        angle -= step;
        if (((angle - goal) & 0xFFF) >= 0x800) {
            angle = goal;
        }
    } else {
        angle += step;
        if (((angle - goal) & 0xFFF) < 0x800) {
            angle = goal;
        }
    }
    return angle & 0xFFF;
}

/* Turn towards `goal` by `step`, or jump there when 800adc18 is set. */
s32 func_80073988(s32 angle, s32 goal, s32 step) {
    s32 result;

    if (D_800ADC18 == 0) {
        result = func_80073930(angle, goal, step);
    } else {
        result = goal & 0xFFF;
    }
    return result;
}

/* The move phase: update the field, point the model light and view angles
 * at the camera, update the camera (on a scratchpad stack), build the view
 * matrix from the shaken eye and target, then turn every drawn actor
 * toward its heading goal and set its model's facing. */
void func_800739C0(void) {
    VECTOR eye;
    VECTOR target;
    FieldActor *actor;
    s32 i;
    s32 step;

    func_8008110C();
    func_8003F738(&D_800B2078.sprite_angles, &D_800AFC30);
    D_800AFC30.t[2] = 0;
    D_800AFC30.t[1] = 0;
    D_800AFC30.t[0] = 0;
    D_800AF880.view_angle = ratan2(D_800AF880.target.vz - D_800AF880.eye.vz, D_800AF880.target.vx - D_800AF880.eye.vx) - 0x400;
    D_800AF880.angle = ratan2(D_800AF880.target_goal.vz - D_800AF880.eye_goal.vz,
                              D_800AF880.target_goal.vx - D_800AF880.eye_goal.vx) - 0x400;
    D_800B00B4 = ratan2(func_80099A4C((D_800AF880.target.vx - D_800AF880.eye.vx) >> 16,
                                      (D_800AF880.target.vz - D_800AF880.eye.vz) >> 16),
                        (D_800AF880.target.vy - D_800AF880.eye.vy) >> 16);
    __asm__ volatile("move $8, %0\n\tsw $29, 0($8)\n\taddiu $8, $8, -4\n\tmove $29, $8"
                     :
                     : "r"(0x1F8003FC)
                     : "$8", "memory");
    func_80073230();
    __asm__ volatile("addiu $29, $29, 4\n\tlw $29, 0($29)" : : : "memory");
    eye.vx = D_800AF880.eye.vx;
    eye.vy = D_800AF880.eye.vy;
    eye.vz = D_800AF880.eye.vz;
    eye.vx += D_800AF880.shake_offset.vx;
    eye.vy += D_800AF880.shake_offset.vy;
    eye.vz += D_800AF880.shake_offset.vz;
    target.vx = D_800AF880.target.vx;
    target.vy = D_800AF880.target.vy;
    target.vz = D_800AF880.target.vz;
    target.vx += D_800AF880.shake_offset.vx;
    target.vy += D_800AF880.shake_offset.vy;
    target.vz += D_800AF880.shake_offset.vz;
    if (D_800ADC18 != 0) {
        func_80073750(&D_800AF880.previous_view, &eye, &target, &D_800AF880.up);
        D_800AF85C = D_800AF880.previous_view;
    } else {
        D_800AF880.previous_view = D_800AF85C;
        func_80073750(&D_800AF85C, &eye, &target, &D_800AF880.up);
    }
    __asm__ volatile("move $8, %0\n\tsw $29, 0($8)\n\taddiu $8, $8, -4\n\tmove $29, $8"
                     :
                     : "r"(0x1F8003FC)
                     : "$8", "memory");
    func_800722F4();
    __asm__ volatile("addiu $29, $29, 4\n\tlw $29, 0($29)" : : : "memory");
    for (i = 0; i < D_800ADBFC; i++) {
        if ((D_800AF880.components.descriptors[i].flags & 0xF40) && !(D_800AF880.components.descriptors[i].flags & 0x20)) {
            actor = D_800AF880.components.descriptors[i].actor;
            if (!(actor->layer_flags & 0x100000) && (actor->layer_flags & 0x600) != 0x200) {
                if (!(actor->flags & 0x8000)) {
                    if (!(actor->unk014 & 0x200000) || (actor->flags & 0x1800)) {
                        if (!(actor->layer_flags & 0x2000)) {
                            step = actor->unk11E;
                        } else {
                            step = D_800B2078.unk21B4;
                        }
                        actor->unk108 = func_80073988(actor->unk108, actor->heading_goal, step);
                    } else {
                        actor->unk108 = func_80073988(actor->unk108, (((actor->unk014 >> 11) - 2) & 7) << 9, 0x200);
                    }
                }
                if (D_800ADB05 == 0) {
                    if (!(actor->layer_flags & 0x01000000)) {
                        func_800223B0(D_800AF880.components.descriptors[i].model,
                                      D_800AF880.view_angle + D_800AF880.components.descriptors[i].actor->unk108);
                    } else {
                        func_80021FE0(D_800AF880.components.descriptors[i].model,
                                      D_800AF880.components.descriptors[i].actor->unk108);
                    }
                }
            }
        }
    }
    if (D_800C268C == 0) {
        func_80281B00("MATRIX    ");
    }
}

/* Refresh each shown model instance's bounds (800aa9dc) and choose its
 * drawing mode from its descriptor flags. */
void func_80073E38(void) {
    s32 i;
    FieldDescriptor *descriptor;
    FieldInstance *instance;
    u16 flags;

    for (i = 0; i < D_800AF880.components.descriptor_count; i++) {
        descriptor = &D_800AF880.components.descriptors[i];
        if (!(descriptor->flags & 0x40)) {
            instance = descriptor->instance;
            func_800AA9DC(instance);
            if (D_800B2078.sprite_gate != 0) {
                if (descriptor->flags & 0x10) {
                    instance->mode = 5;
                } else {
                    instance->mode = 4;
                }
            } else {
                flags = descriptor->flags;
                if (flags & 0xC) {
                    instance->mode = 1;
                } else if (flags & 0x4000) {
                    instance->mode = 3;
                } else if (flags & 0x10) {
                    instance->mode = 2;
                } else {
                    instance->mode = 0;
                }
            }
        }
    }
}

/* Switch to the other draw block and clear its overlay ordering table.
 * Breaks (code 1 in the high field; maspsx places `break N` in the low
 * field, so it is written as 1024) when 800c268c is clear. */
void func_80073F50(void) {
    if (D_800C268C == 0) {
        __asm__ volatile("break 1024");
    }
    D_800ADB08 = (D_800ADB08 + 1) % 2;
    D_800C426C = &D_800B249C[D_800ADB08];
    ClearOTagR(D_800C426C->overlay_ot, 8);
}

/* Swap the draw buffer and clear its ordering tables. */
void func_80073FE0(void) {
    func_80073F50();
    ClearOTagR(D_800C426C->ot, 0x1000);
    if (D_800ADB4C != 0) {
        ClearOTagR(D_800C426C->ot2, 0x1000);
    }
}

/* Copy a matrix's rotation and translation. */
void func_80074038(MATRIX *to, MATRIX *from) {
    func_8007409C(to, from);
    func_80074078(to, from);
}

/* Copy a matrix's translation. */
void func_80074078(MATRIX *to, MATRIX *from) {
    to->t[0] = from->t[0];
    to->t[1] = from->t[1];
    to->t[2] = from->t[2];
}

/* Copy a matrix's rotation. */
void func_8007409C(MATRIX *to, MATRIX *from) {
    to->m[0][0] = from->m[0][0];
    to->m[0][1] = from->m[0][1];
    to->m[0][2] = from->m[0][2];
    to->m[1][0] = from->m[1][0];
    to->m[1][1] = from->m[1][1];
    to->m[1][2] = from->m[1][2];
    to->m[2][0] = from->m[2][0];
    to->m[2][1] = from->m[2][1];
    to->m[2][2] = from->m[2][2];
}

/* Draw the compass: upload its palette (rows of blocked heading octants
 * black), look at it from the camera's height and distance, turn the
 * needle toward the controlled actor's heading, and draw the needle,
 * letters, ring and pointer quads; also leave the upright view in the
 * model pass matrix. */
void func_80074108(void) {
    MATRIX base;
    MATRIX look;
    MATRIX turn;
    MATRIX placed;
    MATRIX tilt;
    SVECTOR angles;
    VECTOR eye;
    VECTOR origin;
    s32 i;
    s32 j;
    u8 blocked;
    s16 view;
    s16 *offset_x;
    s16 *offset_z;

    blocked = D_800AF880.heading_blocks[1];
    for (i = 0; i < 8; i++) {
        if (blocked & D_800ADC24[i]) {
            for (j = 0; j < 16; j++) {
                D_800AFD24[i * 16 + j] = 0;
            }
        } else {
            for (j = 0; j < 16; j++) {
                D_800AFD24[i * 16 + j] = D_800AFC08[j];
            }
        }
    }
    D_800B004C.w = 0x80;
    LoadImage(&D_800B004C, (u_long *)D_800AFD24);
    SetGeomScreen(0x80);
    SetGeomOffset(0x10A, 0xA6);
    origin.vx = 0;
    origin.vy = 0;
    origin.vz = 0;
    eye.vx = 0;
    eye.vy = D_800AF880.eye.vy - D_800AF880.target.vy;
    eye.vz = -func_80099A4C((D_800AF880.eye.vx - D_800AF880.target.vx) >> 16,
                            (D_800AF880.eye.vz - D_800AF880.target.vz) >> 16) << 16;
    func_80073750(&look, &eye, &origin, &D_800AF880.up);
    func_80070594(&base);
    base.t[2] = 0x80;
    SetRotMatrix(&base);
    SetTransMatrix(&base);
    view = D_800AF880.view_angle + 0x400;
    D_800ADB4A = D_800AF880.components.descriptors[D_800B2078.unk233E].actor->heading_goal + view;
    D_800ADB48 = func_80073988(D_800ADB48, D_800ADB4A, 0x40);
    angles.vx = 0;
    angles.vy = D_800ADB48;
    angles.vz = 0;
    func_80072140(&turn);
    func_8003F738(&angles, &turn);
    MulMatrix2(&look, &turn);
    turn.t[2] = 0x1000;
    CompMatrix(&base, &turn, &placed);
    if (D_800B2078.script_control[1] == 0 && D_800ADC18 == 0 && D_8004F378 == 0) {
        for (i = 20; i < 21; i++) {
            func_8007AB6C(D_800C426C->overlay_ot, &D_800B06BC[i], &placed, D_800ADB08);
        }
    }
    func_80070594(&turn);
    MulMatrix2(&look, &turn);
    turn.t[2] = 0x1000;
    CompMatrix(&base, &turn, &placed);
    MulMatrix0(&base, &turn, &D_800AF880.unk204);
    SetRotMatrix(&base);
    SetTransMatrix(&base);
    func_80070594(&turn);
    MulMatrix2(&D_800AF880.previous_view, &turn);
    turn.t[2] = 0x1000;
    CompMatrix(&base, &turn, &placed);
    func_80074038(&base, &placed);
    angles.vx = 0x400;
    angles.vy = 0;
    angles.vz = 0;
    func_8003F738(&angles, &tilt);
    if (D_800B2078.script_control[1] == 0 && D_800ADC18 == 0 && D_8004F378 == 0) {
        for (i = 16; i < 20; i++) {
            func_80070594(&turn);
            offset_x = &D_800ADC34[i - 16].vx;
            offset_z = &D_800ADC34[i - 16].vy;
            turn.t[0] = *offset_x;
            turn.t[2] = *offset_z;
            CompMatrix(&base, &turn, &placed);
            func_8007409C(&placed, &tilt);
            func_8007AC58(D_800C426C->overlay_ot, &D_800B06BC[i], &placed, D_800ADB08);
        }
        for (i = 0; i < 16; i++) {
            func_8007AB6C(D_800C426C->overlay_ot, &D_800B06BC[i], &base, D_800ADB08);
        }
        for (i = 21; i < 25; i++) {
            func_8007AB6C(D_800C426C->overlay_ot, &D_800B06BC[i], &base, D_800ADB08);
        }
    }
    addPrim(D_800C426C->overlay_ot, D_800B1E00[D_800ADB08]);
    SetGeomOffset(0xA0, 0x70);
    SetGeomScreen(D_800AF880.projection);
}

/* Whether any shown descriptor (flag 0x40 clear) has flag 0x8000. */
s32 func_8007469C(void) {
    s32 i;
    u16 flags;

    for (i = 0; i < D_800AF880.components.descriptor_count; i++) {
        flags = D_800AF880.components.descriptors[i].flags;
        if (!(flags & 0x40)) {
            if (flags & 0x8000) {
                return 1;
            }
        }
    }
    return 0;
}

/* Drain the pad queue into this frame's held, pressed and repeated buttons
 * of both ports; port 1's are masked by the event input mask and the
 * position mask, and all are cleared while the camera cuts. */
void func_80074700(void) {
    D_800AFE9C = 0;
    D_800AFEA0 = 0;
    D_800C2694 = 0;
    D_800C38F8 = 0;
    D_800C3900 = 0;
    D_800C3908 = 0;
    while (func_80035CDC() != 0) {
        D_800AFE9C |= D_80059570 & D_800B2078.input_mask;
        D_800AFEA0 |= D_80059574;
        D_800C2694 |= D_8005948C & D_800B2078.input_mask;
        D_800C38F8 |= D_80059490;
        D_800C3900 |= D_800594A4 & D_800B2078.input_mask;
        D_800C3908 |= D_800594A8;
    }
    D_800AFE9C &= D_800ADB00;
    D_800C2694 &= D_800ADB00;
    D_800C3900 &= D_800ADB00;
    func_80035DB0();
    func_8007AE78(1, D_80065848);
    if (D_800ADC18 != 0) {
        D_800AFE9C = 0;
        D_800AFEA0 = 0;
        D_800C2694 = 0;
        D_800C38F8 = 0;
        D_800C3900 = 0;
        D_800C3908 = 0;
    }
    if (D_800ADBDC == 0) {
        D_800C2694 &= ~0x80;
    }
}

/* Draw the field models: set the fog, then for every drawn descriptor build
 * its model matrix (turned about one axis, posed by the 801e bone routine,
 * attached to another descriptor, or placed in the view with the piece
 * drift and orientation modes), light it, and draw its instance unless it
 * is culled. */
void func_800748E8(void) {
    SVECTOR angles;
    VECTOR scale;
    VECTOR half;
    MATRIX view;
    MATRIX placed;
    MATRIX work;
    s32 always;
    FieldDescriptor *descriptor;
    FieldInstance *instance;
    FieldActor *actor;
    u16 pose;
    s32 i;
    s32 orient;

    D_80059578 = 0;
    D_800595C0 = 0;
    if (D_800B2078.sprite_gate != 0) {
        func_8002C6E0(D_800B2078.fog_color[0], D_800B2078.fog_color[1], D_800B2078.fog_color[2]);
        func_8004A10C(D_800B2078.far_color[0], D_800B2078.far_color[1], D_800B2078.far_color[2]);
        SetFogNearFar(D_800B2078.fog_range[0], D_800B2078.fog_range[1], D_800AF880.projection);
    }
    half.vx = 0x800;
    half.vy = 0x800;
    half.vz = 0x800;
    scale.vx = D_800AF880.scale;
    scale.vy = D_800AF880.scale;
    scale.vz = D_800AF880.scale;
    ScaleMatrix(&D_800AF880.unk204, &scale);
    CompMatrix(&D_800AF880.scaled_world, &D_800AFC30, &view);
    D_80050104 = 0;
    D_800B2078.unk21BC[0] += D_800B2078.piece_drift[0];
    D_800B2078.unk21BC[1] += D_800B2078.piece_drift[2];
    D_800B2078.unk21BC[2] += D_800B2078.piece_drift[1];
    for (i = 0; i < D_800AF880.components.descriptor_count; i++) {
        D_800AF880.components.descriptors[i].transform = D_800AF880.components.descriptors[i].matrix;
        always = 0;
        if (D_800AF880.components.descriptors[i].flags & 0x40) {
            continue;
        }
        descriptor = &D_800AF880.components.descriptors[i];
        if (i < D_800ADBFC) {
            switch (descriptor->actor->state.bits.mode) {
            case 1:
                angles.vx = descriptor->actor->unk70;
                angles.vy = 0;
                angles.vz = 0;
                goto turn;
            case 2:
                angles.vx = 0;
                angles.vy = descriptor->actor->unk70;
                angles.vz = 0;
                goto turn;
            case 3:
                angles.vx = 0;
                angles.vy = 0;
                angles.vz = descriptor->actor->unk70;
            turn:
                func_8003F738(&angles, &work);
                MulMatrix2(&D_800AF880.components.descriptors[i].matrix, &work);
                work.t[0] = D_800AF880.components.descriptors[i].matrix.t[0];
                work.t[1] = D_800AF880.components.descriptors[i].matrix.t[1];
                work.t[2] = D_800AF880.components.descriptors[i].matrix.t[2];
                CompMatrix(&D_800AF880.scaled_world, &work, &placed);
                break;
            default:
                actor = descriptor->actor;
                pose = actor->unk128;
                if (pose != 0xFFFF) {
                    func_801E72CC(&D_800AF880.components.descriptors[i].transform,
                                  &D_800AF880.components.descriptors[i].transform, pose >> 12, pose & 0xFFF);
                    CompMatrix(&D_800AF880.scaled_world, &D_800AF880.components.descriptors[i].transform, &work);
                    CompMatrix(&work, &D_800AF880.components.descriptors[i].matrix, &placed);
                    CompMatrix(&D_800AF880.components.descriptors[i].transform,
                               &D_800AF880.components.descriptors[i].matrix,
                               &D_800AF880.components.descriptors[i].transform);
                } else if (actor->unk075 != 0xFF) {
                    CompMatrix(&D_800AF880.scaled_world,
                               &D_800AF880.components.descriptors[D_800AF880.components.descriptors[i].actor->unk075].transform,
                               &work);
                    CompMatrix(&work, &D_800AF880.components.descriptors[i].matrix, &placed);
                    CompMatrix(&D_800AF880.components.descriptors[D_800AF880.components.descriptors[i].actor->unk075].transform,
                               &D_800AF880.components.descriptors[i].matrix,
                               &D_800AF880.components.descriptors[i].transform);
                } else {
                    goto general;
                }
                break;
            }
        } else {
            if (!(D_800B2078.piece_drift_mode & 0x7F)) {
                descriptor->matrix.t[0] += D_800B2078.piece_drift[0];
                descriptor->matrix.t[1] += D_800B2078.piece_drift[2];
                descriptor->matrix.t[2] += D_800B2078.piece_drift[1];
            }
        general:
            if ((D_800B2078.piece_drift_mode & 0x7F) == 1) {
                descriptor->matrix.t[0] += D_800B2078.piece_drift[0];
                descriptor->matrix.t[1] += D_800B2078.piece_drift[2];
                descriptor->matrix.t[2] += D_800B2078.piece_drift[1];
            }
            if (D_800B2078.piece_drift_mode & 0x80) {
                always = 1;
            }
            gte_CompMatrix(&view, &descriptor->matrix, &placed);
            orient = D_800AF880.components.descriptors[i].flags & 3;
            if (orient != 0) {
                if (orient == 1) {
                    MulMatrix0(&D_800AF880.unk204, &D_800AF880.components.descriptors[i].matrix, &placed);
                } else {
                    func_8007409C(&placed, &D_800AF880.components.descriptors[i].matrix);
                    ScaleMatrix(&placed, &scale);
                }
                MulMatrix2(&D_800AF880.orbit, &placed);
            }
        }
        instance = descriptor->instance;
        if (instance->mode == 1) {
            work = placed;
            ScaleMatrix(&work, &half);
            func_80030B14(&work);
            func_80030C40(D_800AF880.back_color[0], D_800AF880.back_color[1], D_800AF880.back_color[2]);
        }
        D_80050104 = 0;
        if (!(D_800AF880.components.descriptors[i].flags & 0x20)) {
            if ((descriptor->flags & 0x2000) && instance->anims != NULL) {
                D_800ADB58 = i;
                D_800ADB5C = 0;
                func_800305D8(instance->anims);
            }
            gte_SetRotMatrix(&placed);
            gte_SetTransMatrix(&placed);
            if (func_800AAA74(instance) == 0 || always == 1) {
                gte_SetRotMatrix(&placed);
                gte_SetTransMatrix(&placed);
                if (!(descriptor->flags & 0x8000)) {
                    func_8002C700(instance->mesh, instance->packets[D_800ADB08], D_800C426C->ot, instance->mode);
                } else {
                    func_8002C700(instance->mesh, instance->packets[D_800ADB08], D_800C426C->ot2, instance->mode);
                }
            }
        }
    }
    if (D_800C268C == 0) {
        func_80281B00("MODEL     ");
    }
}

/* Draw the 801e module's layer (with the emitters updated and its back
 * colour set) when enabled, then the debug "GEAR" timer. */
void func_8007520C(void) {
    if (D_8004F380 == 0) {
        if (D_800B2078.unk2264 != 0) {
            func_80086BA8();
            SetBackColor(D_800B2078.unk225C[0], D_800B2078.unk225C[1], D_800B2078.unk225C[2]);
            func_801E7D14(&D_800AF880.scaled_world, D_800B2078.unk221C, D_800C426C->ot, D_800ADB08, 1);
        }
        if (D_800C268C == 0) {
            func_80281B00("GEAR      ");
        }
    }
}

void func_80024FE4(u32 *ot);
void func_80024FF4(void *p);
void func_8001D468(void);
void func_8001C9F8(void);
void func_8001C964(void);
void func_80023210(FieldModel *model);
void func_80075B44(u32 *ot, s32 buffer);
void func_800764B4(u32 *ot, s32 buffer);

/* Draw the field characters: set up the model renderer for this buffer, then
 * draw each actor's model (shown ones unless their layer is hidden or they
 * are flagged off, others only with layer flag 0x1000000), then the debug
 * "CHAR" timer. The descriptor flags are read as a whole word here. */
void func_800752C8(void) {
    s32 i;

    if (D_800ADB05 == 1) {
        return;
    }
    func_800250E0(D_800ADB08);
    func_80024FE4(D_800C426C->ot);
    func_80024FF4(D_800AFA64);
    func_8001D468();
    func_8001C9F8();
    func_8001C964();
    func_80075B44(D_800C426C->ot, D_800ADB08);
    for (i = 0; i < D_800ADBFC; i++) {
        if ((*(u32 *)&D_800AF880.components.descriptors[i].flags & 0x60) == 0x40) {
            if ((D_800AF880.components.descriptors[i].actor->layer_flags & 0x600) != 0x200
                && !(D_800AF880.components.descriptors[i].actor->layer_flags & 0x1000)
                && !(D_800AF880.components.descriptors[i].actor->flags & 1)) {
                func_80023210(D_800AF880.components.descriptors[i].model);
            }
        } else if (D_800AF880.components.descriptors[i].actor->layer_flags & 0x1000000) {
            func_80023210(D_800AF880.components.descriptors[i].model);
        }
    }
    func_800764B4(D_800C426C->ot, D_800ADB08);
    if (D_800C268C == 0) {
        func_80281B00("CHAR      ");
    }
}

/* Link a table's primitives into `ot` (AddPrims). */
void func_80075458(void *ot, u32 *table, s32 depth) {
    AddPrims(ot, table + depth, table);
}

/* Draw the 801e-layer object from the camera eye and target when event
 * parameters are enabled. */
void func_80075484(void) {
    SVECTOR eye;
    SVECTOR target;

    if (D_800B0080.enabled != 0 && D_800ADB50 == 0) {
        eye.vx = D_800AF880.eye.vx >> 16;
        eye.vy = D_800AF880.eye.vy >> 16;
        eye.vz = D_800AF880.eye.vz >> 16;
        target.vx = D_800AF880.target.vx >> 16;
        target.vy = D_800AF880.target.vy >> 16;
        target.vz = D_800AF880.target.vz >> 16;
        func_800273C4(D_800B007C, &eye, &target, &D_800AF880.scaled_world,
                      D_800C426C->ot + D_800B2078.unk21D4 + 0x1000, D_800ADB08);
    }
}

/* One field frame: the move phase, then drawing effects, dialogue, the
 * compass, characters and models (on a scratchpad stack) and the other
 * layers; flush, clear or copy the next buffer, put its environments,
 * link the depth tables and draw them, then wait out the frame rate. */
void func_8007554C(void) {
    RECT rect;
    s32 start;
    s32 now;
    s32 frames;

    D_800ADB9C = VSync(1);
    start = VSync(-1);
    func_800739C0();
    func_80086908();
    if (D_800C268C == 0) {
        func_80281B00("SEFFECT   ");
    }
    func_80071CB4(D_800C426C->overlay_ot, D_800ADB08);
    if (D_800C268C == 0) {
        func_80281B00("MESSAGE   ");
    }
    func_80074108();
    __asm__ volatile("move $8, %0\n\tsw $29, 0($8)\n\taddiu $8, $8, -4\n\tmove $29, $8"
                     :
                     : "r"(0x1F8003FC)
                     : "$8", "memory");
    func_800748E8();
    func_800752C8();
    func_800A9688();
    if (D_800C268C == 0) {
        func_80281450();
    }
    func_800A4DAC();
    __asm__ volatile("addiu $29, $29, 4\n\tlw $29, 0($29)" : : : "memory");
    func_800A84C0();
    func_80075484();
    func_8007520C();
    func_800ABEC8();
    if (D_800C268C == 0) {
        func_80281400();
        func_80281B00("FntPrint  ");
    }
    D_800ADBA0 = VSync(1);
    DrawSync(0);
    func_800805F4();
    func_8008004C(D_800C426C->overlay_ot, D_800ADB08);
    VSync(0);
    func_80032CB8();
    if (D_800ADC18 == 0) {
        ClearImage(&D_800C426C->draw2.clip, D_800B2078.clear_color[0], D_800B2078.clear_color[1],
                   D_800B2078.clear_color[2]);
    } else if (D_800B0048 == 3) {
        rect.x = 0x2C0;
        rect.y = 0x100;
        rect.w = 0x140;
        rect.h = 0xE0;
        MoveImage(&rect, 0, D_800ADB08 << 8);
    } else {
        ClearImage(&D_800C426C->draw2.clip, 0, 0, 0);
    }
    PutDispEnv(&D_800C426C->disp);
    PutDrawEnv(&D_800C426C->draw);
    if (D_800C268C == 0) {
        D_800ADB9C = VSync(1);
    }
    func_80025044();
    if (D_800C268C == 0) {
        func_80281B00("ShapeTrans");
    }
    func_800920D8();
    if (D_800ADBB4 != 0) {
        LoadImage(&D_800AFC58, (u_long *)D_800AF87C);
        D_800ADBB4 = 0;
    }
    if (D_800C268C == 0) {
        func_80281B00("LineScroll");
    }
    if (D_800ADC18 == 0) {
        if (D_800ADB4C != 0) {
            func_80075458(&D_800C426C->ot[D_800B2078.unk21D4], D_800C426C->ot2, D_800B2078.unk21D4);
        }
        func_80075458(&D_800C426C->overlay_ot[7], D_800C426C->ot, D_800B2078.unk21D4);
    }
    DrawOTag(&D_800C426C->overlay_ot[7]);
    do {
        now = VSync(-1);
        frames = D_800B2078.unk217C + 2;
    } while (now < start + frames);
}

/* Finish the frame: draw the overlays, flush, sync, copy the shown half,
 * then put this block's environments and draw its overlay table. */
void func_80075910(void) {
    RECT rect;

    func_80074700();
    func_800A2030();
    func_800805F4();
    func_8008004C(D_800C426C->overlay_ot, D_800ADB08);
    DrawSync(0);
    VSync(0);
    rect.x = 0x140;
    rect.w = 0x140;
    rect.h = 0xE0;
    rect.y = ((D_800ADB78 + 1) & 1) << 8;
    MoveImage(&rect, 0, D_800ADB08 << 8);
    PutDispEnv(&D_800C426C->disp);
    PutDrawEnv(&D_800C426C->draw);
    DrawOTag(&D_800C426C->overlay_ot[7]);
}

void func_8004A480(VECTOR *a, VECTOR *b, VECTOR *out); /* OuterProduct12 */

/* The rotation matrix whose second row is `axis`: the first row is the unit
 * vector perpendicular to world up and `axis`, the third completes the basis. */
void func_800759E4(MATRIX *m, VECTOR *axis) {
    VECTOR up = { 0, 0, 0x1000, 0 };
    VECTOR side;
    VECTOR cross;

    func_8004A480(&up, axis, &cross);
    VectorNormal(&cross, &side);
    func_8004A480(&side, axis, &cross);
    VectorNormal(&cross, &up);
    m->m[0][0] = side.vx;
    m->m[0][1] = side.vy;
    m->m[0][2] = side.vz;
    m->m[1][0] = axis->vx;
    m->m[1][1] = axis->vy;
    m->m[1][2] = axis->vz;
    m->m[2][0] = up.vx;
    m->m[2][1] = up.vy;
    m->m[2][2] = up.vz;
}

/* Pass a colour on to resident 80021b98 unless 800b218e is set. */
void func_80075B08(void *target, u8 *color) {
    if (D_800B2078.sprite_gate == 0) {
        func_80021B98(target, color[0], color[1], color[2]);
    }
}

/* Draw the sprite actors: keep each one's previous placement, project it to
 * set its off-screen flag, then scale, fog and draw its sprite at its depth
 * in `ot` (layered sprites twice, split sprites in two parts); party actors
 * drawn by the 801e module get their layer object's state instead. */
void func_80075B44(u32 *ot, s32 buffer) {
    SVECTOR v;
    SVECTOR raised;
    VECTOR scale;
    MATRIX placed;
    MATRIX unused; /* unreferenced, but part of the frame */
    MATRIX orient;
    CVECTOR color;
    s32 sxy;
    s32 interpolation;
    s32 flag;
    s32 depth;
    s32 elevation;
    s32 octant;
    s32 upper;
    s32 i;
    s32 party;
    u16 kind;
    s32 x;
    s32 y;
    u32 layer_flags;
    FieldActor *actor;
    FieldModel *sprite;
    u32 side;

    elevation = (s16)D_800AF880.elevation;
    octant = func_8009A514() & 0xFFFF;
    func_8007409C(&orient, &D_800AF880.orbit);
    raised.vx = 0;
    raised.vz = 0;
    raised.vy = -(elevation / 3 * 2);
    party = 0;
    for (i = 0; i < D_800ADBFC; i++) {
        kind = D_800AF880.components.descriptors[i].flags;
        if (!(kind & 0x40)) {
            continue;
        }
        actor = D_800AF880.components.descriptors[i].actor;
        sprite = D_800AF880.components.descriptors[i].model;
        layer_flags = actor->layer_flags;
        D_800AF880.components.descriptors[i].transform = D_800AF880.components.descriptors[i].matrix;
        if (!(layer_flags & 0x2000)) {
            gte_CompMatrix(&D_800AF880.scaled_world, &D_800AF880.components.descriptors[i].matrix, &placed);
            gte_SetRotMatrix(&placed);
            gte_SetTransMatrix(&placed);
            gte_RotTransPers(&raised, &sxy, &interpolation, &flag, &depth);
            y = sxy >> 16;
            x = (s16)sxy;
            if ((u32)(y + 9) < 0x143 && (u32)(x + 0x27) < 0x18F) {
                actor->layer_flags &= ~0x200;
            } else {
                actor->layer_flags |= 0x200;
            }
            if (D_8004F37C != 0 || (kind & 0x20) || flag < 0) {
                continue;
            }
            scale.vx = actor->scale[0] * 3 >> 2;
            scale.vy = actor->scale[1] * 3 >> 2;
            scale.vz = actor->scale[2] * 3 >> 2;
            if (actor->unkE4 == 7 && D_800B2078.unk2268 != 0) {
                scale.vx = scale.vx * 5 >> 2;
                scale.vy = scale.vy * 5 >> 2;
                scale.vz = scale.vz * 5 >> 2;
            }
            sprite->renderer->matrix = orient;
            ScaleMatrix(&sprite->renderer->matrix, &scale);
            if (D_800AF880.components.descriptors[i].actor->unk014 & 0x200000) {
                side = (octant - (((D_800AF880.components.descriptors[i].actor->unk014 >> 11) - 2) & 7)) & 7;
                if (side != 0) {
                    if (side < 4) {
                        v.vx = 0;
                        v.vy = -0x80;
                        v.vz = 0;
                        depth = RotTransPers(&v, (long *)&sxy, (long *)&interpolation, (long *)&flag);
                    } else if (side < 8) {
                        if (side >= 5) {
                            v.vx = 0;
                            v.vy = 0x80;
                            v.vz = 0;
                            depth = RotTransPers(&v, (long *)&sxy, (long *)&interpolation, (long *)&flag);
                        }
                    }
                }
            }
            if (D_800B2078.unk2357 == 0 && D_800B2078.sprite_gate != 0) {
                gte_ldrgb(&D_80059598);
                gte_dpcs();
                gte_strgb(&color);
                func_80021B98(D_800AF880.components.descriptors[i].model, color.r, color.g, color.b);
            }
            depth >>= D_80050100;
            if (depth >= 2) {
                depth -= 2;
            }
            if ((u16)(actor->unkE8 + 0x22) < 2) {
                if (!(actor->layer_flags & 0x02000000)) {
                    func_80021B98(sprite, actor->color0[0], actor->color0[1], actor->color0[2]);
                    sprite->unk3D = 0xEF;
                    func_8001E298(sprite, ot + depth - 0x10);
                    v.vx = 0;
                    v.vy = 300;
                    v.vz = 0;
                    upper = RotTransPers(&v, (long *)&sxy, (long *)&interpolation, (long *)&flag) >> D_80050100;
                    func_80021B98(sprite, actor->color1[0], actor->color1[1], actor->color1[2]);
                    sprite->unk3D = 0xF7;
                    func_8001E298(sprite, ot + upper);
                }
            } else {
                sprite->unk3D = 0;
                if (!(actor->layer_flags & 0x02000000)) {
                    if (!(actor->unk134 & 0x60)) {
                        func_80075B08(sprite, actor->color0);
                        func_8001E298(sprite, ot + depth);
                    } else {
                        if ((actor->unk134 >> 5) & 1) {
                            func_80075B08(sprite, actor->color0);
                            v.vx = 0;
                            v.vy = (actor->unkEE - elevation / 3) * 2;
                            v.vz = 0;
                            upper = RotTransPers(&v, (long *)&sxy, (long *)&interpolation, (long *)&flag) >> D_80050100;
                            if (upper >= 2) {
                                upper -= 2;
                            }
                            func_8001E2F8(sprite, ot + upper, actor->unkEE);
                        }
                        if ((actor->unk134 >> 5) & 2) {
                            func_80075B08(sprite, actor->color1);
                            func_8001E368(sprite, ot + depth, actor->unkEE);
                        }
                    }
                }
            }
        } else if (D_8004F380 == 0) {
            if (!(actor->flags & 0x10000) && !(actor->unk014 & 0x200002) && !(actor->layer_flags & 0x800)) {
                D_801E8670[party]->unk4A &= 0xFFFE;
            } else {
                D_801E8670[party]->unk4A |= 1;
            }
            if (!(kind & 0x20)) {
                D_801E8670[party]->active = 1;
            } else {
                D_801E8670[party]->active = 0;
            }
            if (!(actor->layer_flags & 0x20000)) {
                D_801E8670[party]->model->facing = actor->unk108 + 0xC00;
            } else {
                actor->heading_goal = actor->unk108 = D_801E8670[party]->model->facing - 0xC00;
            }
            D_801E8670[party]->scale = (actor->scale[0] * D_800B2078.layer_depths[party]) >> 12;
            D_801E8670[party]->y = actor->position[1] >> 16;
            D_801E8670[party]->model->x = actor->position[0] >> 16;
            D_801E8670[party]->model->z = actor->position[2] >> 16;
            party++;
            actor->layer_flags &= ~0x200;
        }
    }
}

/* Draw the drop shadows: for every visible sprite actor build a matrix that
 * lays the shadow quad on the floor under it (its axes from the floor
 * normal), scale it by the actor's size, and link the projected quad into
 * `ot`. */
void func_800764B4(u32 *ot, s32 buffer) {
    VECTOR up;
    VECTOR side;
    VECTOR cross;
    MATRIX floor;
    MATRIX placed;
    MATRIX world;
    VECTOR scale;
    s32 interpolation;
    s32 flag;
    s32 depth;
    FieldDescriptor *descriptor;
    FieldActor *actor;
    s32 i;
    s32 sx;
    s32 sy;
    s32 sz;

    world = D_800AF880.scaled_world; /* an unused copy */
    if (D_8004F37C != 0) {
        return;
    }
    for (i = 0; i < D_800ADBFC; i++) {
        descriptor = &D_800AF880.components.descriptors[i];
        if ((DESCRIPTOR_FLAGS_WORD(descriptor) & 0x60) != 0x40) {
            continue;
        }
        actor = descriptor->actor;
        if (actor->layer_flags & 0x102200) {
            continue;
        }
        if (actor->layer_flags & 0x800) {
            continue;
        }
        if (actor->flags & 0x10000) {
            continue;
        }
        if (actor->unk014 & 0x200002) {
            continue;
        }
        up.vx = 0;
        up.vy = 0;
        up.vz = 0x1000;
        gte_OuterProduct12(&up, descriptor->actor->unk50, &cross);
        VectorNormal(&cross, &side);
        gte_OuterProduct12(&side, descriptor->actor->unk50, &cross);
        VectorNormal(&cross, &up);
        floor.m[0][0] = side.vx;
        floor.m[0][1] = side.vy;
        floor.m[0][2] = side.vz;
        floor.m[1][0] = descriptor->actor->unk50[0];
        floor.m[1][1] = descriptor->actor->unk50[1];
        floor.m[1][2] = descriptor->actor->unk50[2];
        floor.m[2][0] = up.vx;
        floor.m[2][1] = up.vy;
        floor.m[2][2] = up.vz;
        floor.t[0] = descriptor->matrix.t[0];
        floor.t[1] = (s16)descriptor->model->unk84;
        floor.t[2] = descriptor->matrix.t[2];
        gte_CompMatrix(&D_800AF880.scaled_world, &floor, &placed);
        sx = descriptor->actor->scale[0] * 0xC00;
        scale.vx = sx >> 12;
        sy = descriptor->actor->scale[1] * 0xC00;
        scale.vy = sy >> 12;
        sz = descriptor->actor->scale[2] * 0xC00;
        scale.vz = sz >> 12;
        if (D_800B2078.unk2268 != 0 && (descriptor->actor->flags & 0x400)) {
            scale.vx = sx >> 14;
            scale.vy = sy >> 14;
            scale.vz = sz >> 14;
        }
        ScaleMatrix(&placed, &scale);
        gte_SetRotMatrix(&placed);
        gte_SetTransMatrix(&placed);
        depth = RotAverage4(&descriptor->shadow->v[0], &descriptor->shadow->v[1], &descriptor->shadow->v[2],
                            &descriptor->shadow->v[3],
                        (long *)&descriptor->shadow->poly[buffer].x0, (long *)&descriptor->shadow->poly[buffer].x1,
                        (long *)&descriptor->shadow->poly[buffer].x2, (long *)&descriptor->shadow->poly[buffer].x3,
                        &interpolation, &flag);
        depth >>= D_80050100;
        addPrim(ot + depth, &descriptor->shadow->poly[buffer]);
    }
}

/* Sprite completion callback: flag the sprite's actor (layer bit 16). */
void func_80076A74(FieldSprite *sprite) {
    D_800AF880.components.descriptors[sprite->sequencer->actor].actor->layer_flags |= 0x10000;
}

/* Create an event actor's sprite: record its slot and arguments on the
 * actor, release any sprite the descriptor had, build the new one (a
 * character sheet in the slot's VRAM area, a banked sheet, or one of the two
 * small effect kinds), place it at the actor and register the completion
 * callback. */
void func_80076AC0(s32 index, s32 slot, void *data, s32 kind, s32 bank, s32 unk, s32 flag) {
    s32 width;
    s32 height;
    s32 depth;
    FieldModel *sprite;
    s32 y;
    s32 x;

    func_80032498(8, 0);
    D_800AF880.components.descriptors[index].actor->unk127 = slot;
    D_800AF880.components.descriptors[index].actor->unk126 = unk;
    D_800AF880.components.descriptors[index].actor->unk134 =
        (D_800AF880.components.descriptors[index].actor->unk134 & ~0xF) | (bank & 0xF);
    D_800AF880.components.descriptors[index].actor->sprite_kind = kind;
    D_800AF880.components.descriptors[index].actor->unk134 =
        (D_800AF880.components.descriptors[index].actor->unk134 & ~0x10) | ((flag & 1) << 4);
    if (kind == 0) {
        y = D_800B1F78.slot[slot].y;
        x = D_800B1F78.slot[slot].x;
        if (bank == 0) {
            if (D_800AF880.components.descriptors[index].unk5A & 1) {
                func_800230A8(D_800AF880.components.descriptors[index].model);
            }
            sprite = func_80024524(data, 0x100, slot + 0x1E0, x, y, 0x40);
            D_800AF880.components.descriptors[index].model = sprite;
        } else {
            if (D_800AF880.components.descriptors[index].unk5A & 1) {
                func_800230A8(D_800AF880.components.descriptors[index].model);
            }
            sprite = func_80024294(data, bank * 16 + 0x100, slot + 0x1E0, x, y, 0x40, bank);
            D_800AF880.components.descriptors[index].model = sprite;
        }
    } else {
        if (D_800AF880.components.descriptors[index].unk5A & 1) {
            func_800230A8(D_800AF880.components.descriptors[index].model);
        }
        if (kind == 1) {
            y = (slot << 6) + 0x100;
            sprite = func_80024524(data, 0x100, slot + 0xE0, 0x280, y, 8);
            D_800AF880.components.descriptors[index].model = sprite;
            func_80023340(sprite, 0x20);
        } else {
            y = (slot << 6) + 0x100;
            sprite = func_80024524(data, 0x100, slot + 0xE3, 0x2A0, y, 8);
            D_800AF880.components.descriptors[index].model = sprite;
            func_80023340(sprite, 0x20);
        }
    }
    D_800AF880.components.descriptors[index].unk5A |= 1;
    func_8001F5BC(sprite, 0, &width, &height, &depth);
    func_80021C00(sprite, 3);
    sprite->unk2C = 0xC00;
    sprite->unk82 = 0x2000;
    if (D_8004F30C == 0) {
        sprite->position[0] = D_800AF880.components.descriptors[index].actor->position[0];
        sprite->position[1] = D_800AF880.components.descriptors[index].actor->position[1];
        sprite->position[2] = D_800AF880.components.descriptors[index].actor->position[2];
        sprite->unk84 = D_800AF880.components.descriptors[index].matrix.t[1];
        sprite->velocity[1] = 0;
        sprite->velocity[0] = 0;
        sprite->velocity[1] = 0;
        sprite->velocity[2] = 0;
        sprite->gravity.value = 0x10000;
        if (kind == 0) {
            D_800AF880.components.descriptors[index].actor->height = height * 2;
        } else {
            D_800AF880.components.descriptors[index].actor->height = 0x40;
        }
    }
    if (D_800B2078.sprite_gate != 0) {
        sprite->unk40 |= 0x40000;
    }
    func_800245D8(sprite, 0);
    func_80021FE0(sprite, 0);
    func_80032498(8, 0);
    sprite->animation->actor = index;
    func_80021BF8(sprite, func_80076A74);
    if (flag == 0) {
        func_80023210(sprite);
        func_8001C964();
        if (sprite->animation->unk0C == 0xFF) {
            D_800AF880.components.descriptors[index].actor->unk0EA = 0xFF;
            D_800AF880.components.descriptors[index].actor->layer_flags |= 0x01000000;
            sprite->position[0] = D_800AF880.components.descriptors[index].actor->position[0];
            sprite->position[1] = D_800AF880.components.descriptors[index].actor->position[1];
            sprite->position[2] = D_800AF880.components.descriptors[index].actor->position[2];
        }
    }
    D_800AF880.components.descriptors[index].transform.t[0] = D_800AF880.components.descriptors[index].matrix.t[0] =
        WHOLE(D_800AF880.components.descriptors[index].actor->position[0]);
    D_800AF880.components.descriptors[index].transform.t[1] = D_800AF880.components.descriptors[index].matrix.t[1] =
        WHOLE(D_800AF880.components.descriptors[index].actor->position[1]);
    D_800AF880.components.descriptors[index].transform.t[2] = D_800AF880.components.descriptors[index].matrix.t[2] =
        WHOLE(D_800AF880.components.descriptors[index].actor->position[2]);
    sprite->unk84 = D_800AF880.components.descriptors[index].matrix.t[1];
    sprite->position[0] = D_800AF880.components.descriptors[index].actor->position[0];
    sprite->position[1] = D_800AF880.components.descriptors[index].actor->position[1];
    sprite->position[2] = D_800AF880.components.descriptors[index].actor->position[2];
    D_800AFC74++;
}

/* Set the semi-transparency bit of each of `count` 16-bit pixels. */
void func_800771B0(u32 *pixels, s32 count) {
    s32 i;

    i = count / 2;
    while (--i != -1) {
        *pixels++ |= 0x80008000;
    }
}

/* Load every image (and CLUT) of a TIM list into VRAM where it lies. */
void func_800771F8(u32 *tim) {
    TIM_IMAGE image;

    OpenTIM(tim);
    while (ReadTIM(&image) != NULL) {
        if (image.caddr != NULL) {
            LoadImage(image.crect, image.caddr);
        }
        if (image.paddr != NULL) {
            LoadImage(image.prect, image.paddr);
        }
    }
}

/* Declared without a prototype: 80084a40 also reads a fifth, stack
 * argument that this caller never passes. */
s32 func_80084A40();

/* Place the party at the controlled actor: run its position pass (80084a40),
 * then give each other party member (slots 1 and 2) the leader's model
 * position and descriptor origin after its own pass, and fill the 32
 * history records. */
void func_80077268(void) {
    FieldDescriptor *descriptor;
    FieldActor *actor;
    FieldModel *model;
    s32 slot;
    s32 i;

    func_80084A40(D_800B2078.controlled,
                  WHOLE(D_800AF880.components.descriptors[D_800B2078.controlled].actor->position[1]),
                  &D_800AF880.components.descriptors[D_800B2078.controlled],
                  D_800AF880.components.descriptors[D_800B2078.controlled].actor);
    for (i = 0; i < D_800ADBFC; i++) {
        descriptor = &D_800AF880.components.descriptors[i];
        actor = descriptor->actor;
        if ((descriptor->flags & 0xF80) == 0x200) {
            slot = func_8009FA00(actor->unkE4);
            if (slot != -1) {
                model = D_800AF880.components.descriptors[i].model;
                if (slot != 0) {
                    func_80084A40(i, WHOLE(D_800AF880.components.descriptors[i].actor->position[1]),
                                  descriptor, actor);
                    model->position[0] = D_800AF880.components.descriptors[D_800B2078.controlled].model->position[0];
                    model->position[1] = D_800AF880.components.descriptors[D_800B2078.controlled].model->position[1];
                    model->position[2] = D_800AF880.components.descriptors[D_800B2078.controlled].model->position[2];
                    descriptor->matrix.t[0] = D_800AF880.components.descriptors[D_800B2078.controlled].matrix.t[0];
                    descriptor->matrix.t[1] = D_800AF880.components.descriptors[D_800B2078.controlled].matrix.t[1];
                    descriptor->matrix.t[2] = D_800AF880.components.descriptors[D_800B2078.controlled].matrix.t[2];
                }
            }
        }
    }
    D_800B2078.history[2] = 0;
    D_800B2078.history[1] = 0;
    D_800B2078.history[0] = 0;
    for (i = 0; i < 0x20; i++) {
        func_80081C54(D_800B2078.controlled);
    }
}

/* Load the text palette, with the debug font first when enabled. */
void func_80077544(void) {
    if (D_800C268C == 0) {
        func_8003747C(0x80270000);
        func_800374E8(0x10, 0x10, 0x130, 0xE0, 0x400, 4, 0x3C0, 0x100, 0x100, 0x1FF, 0);
    }
    func_80033698(0x100, 0xF0);
}

/* Select the field's heap tag and directory, then set up the pointer. */
void func_800775C0(void) {
    func_80032498(8, 0);
    func_80028470(4, 0);
    func_80071EE8();
}

/* Wait for drawing to finish (DrawSync), then VSync. */
void func_800775F8(void) {
    DrawSync(0);
    VSync(0);
}

extern s32 D_8004F344;       /* 1 while the text-image file is already loaded */
extern s32 *D_8005A4A0;      /* the text-image file (a7) */
extern RECT D_800B004C;      /* compass colour strip */
extern u16 D_800AFC08[16];   /* compass colours read back from VRAM */
extern s16 D_800C2690;
extern s16 D_800C2692;
extern s16 D_800C38FC;
extern s16 D_800C38FE;
void func_8003342C(void *table);
void func_80070340(u32 *tim, s16 x, s16 y, s16 clut_x, s16 clut_y, s16 clut_w, s16 clut_h);

/* Load the field's text images (file a7, read once while 8004f344 is clear):
 * relocate its offset table, load its eight TIMs where 800adc44 places them
 * (x, y, palette x, y, w, h), read the compass colours back from VRAM (0, fb)
 * and release the file. */
void func_80077620(void) {
    u32 **tim;
    s32 i;

    if (D_8004F344 == 0) {
        D_8005A4A0 = func_80031BDC(func_800288EC(0xA7), 1);
        func_800320A4(D_8005A4A0);
        func_800295D8(0xA7, D_8005A4A0, 0, 0x80);
        func_80028A60(0);
    }
    func_800320B8(D_8005A4A0);
    D_8004F344 = 0;
    D_800C2692 = 0;
    D_800C2690 = 0;
    D_800C38FE = 0;
    D_800C38FC = 0;
    func_8003342C(D_8005A4A0);
    tim = (u32 **)D_8005A4A0;
    for (i = 0; i < 8; i++) {
        func_80070340(*++tim, D_800ADC44[i * 6], D_800ADC44[i * 6 + 1], D_800ADC44[i * 6 + 2],
                      D_800ADC44[i * 6 + 3], D_800ADC44[i * 6 + 4], D_800ADC44[i * 6 + 5]);
        DrawSync(0);
    }
    D_800B004C.x = 0;
    D_800B004C.y = 0xFB;
    D_800B004C.w = 0x10;
    D_800B004C.h = 1;
    StoreImage(&D_800B004C, (u32 *)D_800AFC08);
    DrawSync(0);
    func_800320E8(D_8005A4A0);
}

/* Stop the stream, then read the map's data ahead until it is in. */
void func_800777DC(void) {
    func_80028A60(0);
    while (func_8001B484((D_8004F34C & 0xFFF) * 2, 0) != 0) {
    }
}

/* Record the VSync counter. */
void func_8007781C(void) {
    D_800ADBA4 = VSync(1);
}

/* Set a matrix's nine rotation elements. */
void func_80077844(MATRIX *m, s32 m00, s32 m01, s32 m02, s32 m10, s32 m11, s32 m12, s32 m20,
                   s32 m21, s32 m22) {
    m->m[0][0] = m00;
    m->m[0][1] = m01;
    m->m[0][2] = m02;
    m->m[1][0] = m10;
    m->m[1][1] = m11;
    m->m[1][2] = m12;
    m->m[2][0] = m20;
    m->m[2][1] = m21;
    m->m[2][2] = m22;
}


/* Load the 801e module and its per-layer resources when the layer is
 * enabled: allocate the module (file 6b9), two blocks per layer (files
 * 6bb and 6ba plus the layer's id), then read them all as one list. */
void func_80077884(void) {
    u32 end;
    s32 i;

    if (D_800B2078.unk2264 != 0) {
        func_8008A520();
        func_80028470(4, 0);
        func_800A90B4(0);
        end = D_800ADB30;
        if (D_8004F370 == 0) {
            D_800ADB20 = func_80031BDC((end & 0xFFFFFF) - 0x1DC008, 1);
        } else {
            D_800ADB20 = func_80031BDC(func_800288EC(0x6B9), 1);
        }
        func_800A90B4(1);
        for (i = 0; i < D_800B2078.unk2264; i++) {
            D_800B2394[i * 2 + 1].file = D_800B2078.unk21DC[i] + 0x6BB;
            D_8005A450[i] = func_80031BDC(func_800288EC(D_800B2078.unk21DC[i] + 0x6BB), 1);
            D_800B2394[i * 2 + 1].destination = D_8005A450[i];
        }
        for (i = 0; i < D_800B2078.unk2264; i++) {
            D_800B2394[i * 2].file = D_800B2078.unk21DC[i] + 0x6BA;
            D_8005A420[i] = func_80031BDC(func_800288EC(D_800B2078.unk21DC[i] + 0x6BA), 0);
            D_800B2394[i * 2].destination = D_8005A420[i];
        }
        D_800B2394[i * 2].file = 0x6B9;
        D_800B2394[i * 2].destination = D_800ADB20;
        D_800B2394[i * 2 + 1].file = 0;
        D_800B2394[i * 2 + 1].destination = 0;
        func_8008A520();
        func_80029AFC(D_800B2394, 0, 0);
    }
}


/* Start the 801e module's layers when enabled: sync and flush the cache,
 * initialise the module, set the back colour, then create each layer from
 * its two resources (releasing the first) and keep its depth. */
void func_80077AB4(void) {
    SVECTOR *angles;
    s32 row;
    s32 i;

    if (D_800B2078.unk2264 != 0) {
        func_8008A520();
        func_8007999C();
        func_801E738C(D_800B2078.unk234A);
        D_801E8644 = D_800B2078.unk223C;
        SetBackColor(D_800B2078.unk225C[0], D_800B2078.unk225C[1], D_800B2078.unk225C[2]);
        for (i = 0; i < D_800B2078.unk2264; i++) {
            angles = &D_800B2078.layer_angles[i];
            angles->vx = 0;
            angles->vy = 0;
            angles->vz = 0;
            row = D_800B2078.unk225F[i];
            func_801E742C(i, 0, D_8005A420[i], D_8005A450[i],
                          (s16)(0x240 - ((i + row) << 6)), 0x100, 0, (s16)(i + 0xFC),
                          angles);
            func_800320E8(D_8005A450[i]);
            D_800B2078.layer_depths[i] = D_801E8670[i]->scale;
        }
        func_80032498(8, 0);
    }
}

/* Run 80077884 then 80077ab4. */
void func_80077C60(void) {
    func_80077884();
    func_80077AB4();
}

/* Allocate and keep the three party sprite blocks (0x14000 bytes each). */
void func_80077C88(void) {
    func_80032498(8, 0);
    D_8005A414[0] = func_80031BDC(0x14000, 0);
    D_8005A414[1] = func_80031BDC(0x14000, 0);
    D_8005A414[2] = func_80031BDC(0x14000, 0);
    func_800320A4(D_8005A414[0]);
    func_800320A4(D_8005A414[1]);
    func_800320A4(D_8005A414[2]);
}

/* Unlink, then release, the three blocks at 8005a414..8005a41c. */
void func_80077D2C(void) {
    func_800320B8(D_8005A414[0]);
    func_800320B8(D_8005A414[1]);
    func_800320B8(D_8005A414[2]);
    func_800320E8(D_8005A414[0]);
    func_800320E8(D_8005A414[1]);
    func_800320E8(D_8005A414[2]);
}

/* "Clear OTAG". The original assembler left a stray byte (0x6b) in the
 * string's alignment padding, so the literal is linked as original rodata. */
INCLUDE_RODATA(".local/decomp/field/asm/nonmatchings/field", D_8006FB80);
extern char D_8006FB80[];

/* Field pre-frame work: record the VSync counter, clear the order table,
 * run 80074700, start the debug "Clear OTAG" timer and 800a31e8. */
void func_80077DAC(void) {
    D_800ADB9C = VSync(1);
    func_80073FE0();
    func_80074700();
    if (D_800C268C == 0) {
        func_80281B00(D_8006FB80);
    }
    func_800A31E8();
}

/* -1 when the field may leave (800adbd0 is 1, 800b2344 clear, and the
 * controlled actor has flag 0x800), else 0. */
s32 func_80077E10(void) {
    if (D_800ADBD0 == 1 && D_800B2078.jump_mode == 0
        && (D_800AF880.components.descriptors[D_800B2078.controlled].actor->flags & 0x800)) {
        return -1;
    }
    return 0;
}

extern s32 D_8004F310;
extern s32 D_8004F2F8;
extern u8 D_800594D0;
extern s32 D_80010000;      /* -1 in the debug environment */
extern s32 D_80059560;
extern s32 D_800595AC;
extern s32 D_8006251C;
extern s32 D_80062524;
extern GameState D_8006D634; /* the game state */
extern s32 D_8004F354;
extern s32 D_8004F358;
extern s32 D_800AFC78;
extern s32 D_8004F31C;
extern s32 D_8004F320;
extern s32 D_80059488;
extern u16 D_800C3900;      /* pad buttons held */
extern u16 D_800C3908;      /* pad buttons pressed */
extern u16 D_800AFE9C;
extern s32 D_8004F334;
extern u8 D_8005954C;
extern s32 D_8004F378;
extern s32 D_8004F37C;
extern u8 D_80059171;
void func_8007781C(void);
void func_80085890(); /* called with an argument it ignores */
void func_802811EC(void);
void func_800A94A4(s32 actor);
s32 func_80035734(s32 a0);
void func_80037EE4(void);
void func_80037E8C(void);
void func_8001FAB4(s32 a0, s32 a1);
void func_80019CA0(void);
void func_800A5924(void);
void func_8007FFE8(void);
void func_800A3F4C(void);
void func_8003A89C(s32 sequence, s32 volume, s32 a2);
void func_800A5C40(void);
void func_800ACE90(void);
void func_800ABA98(void);
void func_800A7C58(void);
void func_800A9460(void);
void func_800864F0(void);
void func_800700B0(void);
void func_80085988(void);

/* The field mode entry: set up the heap and the debug hooks, take the map
 * and music from the game state, run the field entry (80078d44), then the
 * frame loop until an exit is requested: pauses (pad start, or the stream
 * stopping), battle requests (with the battle music), the queued map, world
 * map and movie exits, menus and debug keys. Leaves through 8007954c with
 * the exit kind. */
void func_80077E88(void) {
    u8 unused[8]; /* never used; the original frame reserves it */
    s32 exit;
    s32 held;
    s32 saved;
    s32 entered;
    u16 pressed;
    u16 buttons;

    if (D_80010000 != -1) {
        D_800C268C = 0;
    } else {
        D_800C268C = 1;
    }
    func_8007999C();
    if (D_800C268C == 0) {
        DrawSyncCallback(func_8007781C);
    }
    D_8006251C = D_80059560;
    D_80062524 = D_800595AC;
    func_80032498(8, 0);
    if (D_800C268C == 0 && D_8004F370 == 0) {
        func_80028470(4, 0);
        func_800295D8(0xAD, (void *)0x80280000, 0, 0x80);
        func_80028A60(0);
        func_8007999C();
    }
    if (D_8004F30C == 0) {
        D_8006FABC[2] = 0xFF;
        D_8006FABC[1] = 0xFF;
        D_8006FABC[0] = 0xFF;
    }
    func_80085890(0);
    held = 0;
    func_80077C88();
    D_800ADBE8 = -1;
    D_800ADBE4 = -1;
    D_800ADBE0 = -1;
    D_800ADBDC = -1;
    D_800ADBD8 = -1;
    D_8004F358 = 0;
    D_8004F354 = 0;
    D_800ADC10 = 0;
    D_800ADB60 = 0;
    D_800ADB34 = 0;
    D_800ADB7C = 0;
    D_800ADC04 = 2;
    if (D_800C268C == 0) {
        func_802811EC();
    }
    func_800775C0();
    D_8005A39C = &D_8006D634;
    D_8004F34C = D_8006D634.unk231A;
    D_8006D634.vars[1] = D_8006D634.unk2320;
    D_8006D634.vars[4] = D_8006D634.unk231C >> 9;
    if (D_8004F2F8 == 0) {
        D_800594D0 = 0;
        D_8004F324 = 0xFF;
    } else {
        D_8004F324 = D_8006D634.unk2322;
    }
    if (D_800C268C == 1) {
        D_8005A39C->vars[0x28] = 1;
        func_800A3074(0x50, 1);
    }
    func_80077620();
    D_8004F320 = 0;
    func_8001B044();
    func_8001B3A8();
    D_800ADB30 = (u32)func_80031BDC(4, 1);
    if (D_800C268C == 0) {
        __asm__ volatile("break 1024");
        func_800A94A4(D_800B2078.controlled);
        D_800B02CC[0].unk00 = 1;
        D_800B02CC[0].count = 0x10;
    }
    entered = 0;
    func_80078D44();
    D_800ADB04 = 1;
    for (;;) {
        if (func_80035734(0) == 0) {
            saved = D_80059488;
            func_80037EE4();
            func_8001FAB4(0x88, (((D_800ADB08 + 1) & 1) << 8) | 0x64);
            do {
                DrawSync(0);
                VSync(2);
                func_80074700();
                func_80019CA0();
            } while (func_80035734(0) == 0);
            func_80037E8C();
            D_80059488 = saved;
        }
        if ((D_800C3900 & 0x800) && !(D_800AFE9C & 0x40) && D_800B2078.unk2358 == 0) {
            saved = D_80059488;
            func_80037EE4();
            func_8001FAB4(0x88, (((D_800ADB08 + 1) & 1) << 8) | 0x64);
            do {
                DrawSync(0);
                VSync(2);
                func_80074700();
                func_80019CA0();
            } while (!(D_800C3900 & 0x800));
            func_80037E8C();
            D_80059488 = saved;
        }
        if (D_800C268C == 1) {
            func_800A3074(0x50, 1);
        }
        func_80019CA0();
        func_80077DAC();
        func_8007554C();
        func_800A5924();
        if (D_800ADB08 == 1 && D_800ADBDC == 0 && func_80078BC8() == 0 && func_80077E10() == 0) {
            if (D_8004F334 != -1) {
                func_800320B8(D_8005A4E0);
                func_800320E8(D_8005A4E0);
            }
            if (entered == 0) {
                entered = 1;
                D_800AFC78 = D_8004F324;
            }
            func_8007FFE8();
            if (D_800ADBD0 == 1) {
                D_8005954C = D_800B2078.unk2355;
                D_800AFC78 = D_8004F324;
                if (D_8004F338 != D_800B2290) {
                    if (D_8004F338 != -1) {
                        D_8004F348 = 1;
                    }
                    func_8001B66C();
                    D_8004F308 = -1;
                    D_8004F324 = D_800B2290;
                    func_80085B20(D_800B2290, 1);
                }
                D_800ADBD0 = 0;
                D_800ADBD4 = 1;
            } else {
                if (D_800ADB18 == 0) {
                    D_8004F30C++;
                    func_800A3F4C();
                }
                exit = 0;
                if (D_800ADBD4 == 1) {
                    func_8003A89C(D_80062528, 0x7F, 0);
                }
                D_800ADBD4 = 0;
                break;
            }
        }
        if (D_800ADBEC == 0 && D_8004F308 == 0 && D_800ADBC4 == 0xFF && D_800ADB90 == 0
            && func_8001B484((D_8004F34C & 0xFFF) * 2, 0) == 0 && func_800286CC() == 0
            && D_800B2078.fades[0].steps == 0) {
            D_800ADB04 = 0;
            func_800A30FC();
            func_80028A60(0);
            func_800A5C40();
            func_80035DB0();
            D_800ADB04 = 1;
        }
        if (D_800ADB08 == 1 && D_800ADBE4 == 0 && func_80078BC8() == 0) {
            func_80028A60(0);
            exit = 1;
            if (D_8004F334 != -1) {
                func_800320B8(D_8005A4E0);
                func_800320E8(D_8005A4E0);
            }
            break;
        }
        if (D_800ADB08 == 1 && D_800ADBE8 == 0 && func_80078BC8() == 0) {
            func_80028A60(0);
            if (D_8004F334 != -1) {
                func_800320B8(D_8005A4E0);
                func_800320E8(D_8005A4E0);
            }
            D_8004F310++;
            exit = 2;
            func_800A3F4C();
            break;
        }
        if (D_800ADB08 == 1 && D_800ADBD8 == 0 && func_80078BC8() == 0) {
            func_80028A60(0);
            if (D_8004F334 != -1) {
                func_800320B8(D_8005A4E0);
                func_800320E8(D_8005A4E0);
            }
            exit = 3;
            func_8001B66C();
            break;
        }
        if (D_800C268C == 0) {
            pressed = D_800C3908;
            if (pressed & 0x40) {
                D_8004F378 = (D_8004F378 + 1) & 1;
            }
            if (pressed & 0x10) {
                D_8004F37C = (D_8004F37C + 1) & 1;
            }
            if (pressed & 0x80) {
                D_8004F380 = (D_8004F380 + 1) & 1;
            }
            if ((D_800AFE9C & 0x40) && (D_800C3900 & 0x100) && D_800ADBEC == -1 && D_8004F308 == 0
                && D_800ADB34 == 0) {
                D_8004F34C = 0;
                D_800ADBEC = 0;
                func_800A3074(2, 0);
            }
        }
        if (D_800ADBD8 == -1 && D_800ADBDC == -1 && D_800ADBE4 == -1 && func_80078BC8() == 0
            && D_800ADBEC == -1) {
            buttons = D_800AFE9C;
            if (!(buttons & 3)) {
                held = 0;
            }
            if ((buttons & 1) && D_800ADB68 == 1 && (buttons & 2) && held == 0) {
                held = 1;
                func_800798BC();
                if (D_800ADB64 == 0xFF
                    && !(D_800AF880.components.descriptors[D_800B2078.controlled].actor->flags & 0x1800)
                    && D_80059179 == 0) {
                    func_800ACE90();
                }
            }
            if ((D_800C3900 & 0x100) && D_800ADBEC == -1 && D_800ADB68 == 1) {
                func_800ABA98();
            }
            if (D_800ADB70 != 0 && D_800ADB08 == 1) {
                func_800A7C58();
                D_800ADB70 = 0;
            }
            if (D_800ADB64 != 0xFF && D_800ADB08 == 0
                && !(D_800AF880.components.descriptors[D_800B2078.controlled].actor->flags & 0x1800)) {
                func_8007FFE8();
                func_800799D4();
                D_800ADB64 = 0xFF;
            }
            if ((D_800C3900 & 0x10) && D_800B2078.script_control[0] == 0 && D_800ADB64 == 0xFF
                && D_800ADB68 == 1) {
                D_800ADB64 = 0x80;
                D_80059171 = D_800B2078.unk236C;
            }
        }
        func_80078B5C();
    }
    func_800798BC();
    func_800A91F0();
    func_800A31E8();
    func_800A9460();
    func_800864F0();
    func_8007FFE8();
    DrawSync(0);
    VSync(0);
    func_800700B0();
    func_80077D2C();
    func_80085988();
    D_8004F31C = 0;
    func_800320E8((void *)D_800ADB30);
    func_8007954C(exit);
}

/* Field post-frame work: 8003fa38, resolve a pending sound, count down the
 * instant-turn frames. Declared int but returns nothing (the return register
 * stays live, so the final branch keeps an empty delay slot). */
s32 func_80078B5C(void) {
    rand();
    if (D_8004F308 == -1) {
        D_8004F308 = func_80085C90(D_8004F324);
    }
    if (D_800ADC18 != 0) {
        D_800ADC18--;
    }
}

/* 0 when no battle menu, disc, music, battle request or pending transition
 * is busy and 800adbc4 is 0xff; otherwise -1. */
s32 func_80078BC8(void) {
    if (D_800ADB2C != 0) {
        return -1;
    }
    if (func_800286CC() == 0 && D_8004F308 == 0 && D_800ADB90 == 0 && D_800ADB34 == 0
        && D_800ADBC4 == 0xFF) {
        return 0;
    }
    return -1;
}

/* With 800b2344 set, brighten the 256x32 text strip at (0, 1e0) in VRAM
 * (every non-transparent pixel gains 0x0c63) and disable both buffers'
 * dithering. */
void func_80078C5C(void) {
    RECT rect;
    u32 *pixels;
    s32 i;

    if (D_800B2078.jump_mode != 0) {
        D_800B249C[0].draw.dtd = 0;
        D_800B249C[1].draw.dtd = 0;
        pixels = func_80031BDC(0x4000, 0);
        rect.y = 0x1E0;
        rect.w = 0x100;
        rect.x = 0;
        rect.h = 0x20;
        StoreImage(&rect, pixels);
        DrawSync(0);
        for (i = 0; i < 0x1000; i++) {
            if (pixels[i] & 0xFFFF) {
                pixels[i] |= 0xC63;
            }
            if (pixels[i] & 0xFFFF0000) {
                pixels[i] |= 0x0C630000;
            }
        }
        LoadImage(&rect, pixels);
        DrawSync(0);
        func_800320E8(pixels);
    }
}

extern s32 D_8004F310;
extern s32 D_8004F2F8;  /* 1 once the field screen has been set up */
extern s32 D_8004F304;  /* music sequence to release on entry */
extern u8 D_8005942C;   /* 1 when entering from battle */
extern u8 D_800594D0;   /* 1 when entering from a movie */
void func_80031FF8(void);
void func_80070CC8(void);
void func_800A24C4(void);
void func_801E7378(s32 on);
/* Declared without prototypes: this caller passes arguments they ignore. */
void func_800A5884();
void func_800A77C4();

/* The field entry: load the text palette and screen, set up both draw
 * buffers, load the map (80070cc8, 80070488), finish the stream read ahead
 * while fading or growing the screen pieces, release the previous music,
 * start the map's music, then run the first frames (8 after a movie, 32
 * with the fade-in otherwise). */
void func_80078D44(void) {
    RECT rect;
    s32 grow;
    s32 shade;
    s32 i;

    func_80077544();
    func_800A915C();
    func_80028470(4, 0);
    func_800777DC();
    func_800775F8();
    if (D_8004F2F8 == 0) {
        func_800A77C4(0);
    }
    func_80071FB0();
    if (D_8004F2F8 == 0) {
        rect.y = 0x100;
        rect.w = 0x140;
        rect.x = 0;
        rect.h = 0xE0;
        MoveImage(&rect, 0, 0);
    }
    D_800ADB08 = 1;
    func_800A4748();
    func_800A476C(0, 0x100);
    DrawSync(0);
    func_80073FE0();
    func_800775F8();
    if (D_800594D0 == 1 || D_8005942C == 1) {
        func_800A5884(0, 0);
    } else {
        func_800A5884(1, 1);
    }
    D_8004F2F8 = 1;
    func_80028A60(0);
    func_80028470(4, 0);
    func_80070CC8();
    func_80070488();
    D_800AFD04 = 1;
    if (D_800B2078.unk2264 != 0) {
        func_801E7378(1);
    }
    if (D_800594D0 == 1 || D_8005942C == 1) {
        grow = 0;
    } else {
        grow = 0x20;
    }
    shade = 0x800000;
    if (D_800ADB60 == 1) {
    stream:
            func_80073FE0();
            func_800A6408();
            func_800A6924();
            if (D_8005942C != 1) {
                if (D_800C2684 < 0x22C0) {
                    D_800C2684 += grow;
                }
            } else {
                func_800A5600(shade >> 16);
                shade -= 0x40000;
                if (shade < 0) {
                    shade = 0;
                }
            }
        if (func_800286CC() != 0) {
            goto stream;
        }
        DrawSync(0);
        func_800320E8(D_800ADC14);
        D_800ADB60 = 0;
        func_80078C5C();
    }
    if (D_8005942C == 1) {
        do {
            func_80073FE0();
            func_800A6408();
            func_800A6924();
            func_800A5600(shade >> 16);
            shade -= 0x40000;
        } while (shade >= 0);
    }
    if (D_800594D0 == 1) {
        rect.w = 0x140;
        rect.y = 0;
        rect.x = 0;
        rect.h = 0xE0;
        MoveImage(&rect, 0x200, 0);
    }
    if (D_8004F304 != 0) {
        func_80039C4C(D_80062528);
        func_800399D4(D_80062528);
        func_80038310(D_8006258C);
        D_8004F304 = 0;
    }
    func_800A24C4();
    D_8004F310 = 0;
    D_8004F30C = 0;
    func_800775F8();
    func_80035DB0();
    D_8004F308 = 0;
    if (D_800594D0 == 1) {
        D_8004F324 = 0xE;
        func_80085EEC();
    }
    if (D_8004F338 != D_8004F324) {
        func_8001B66C();
        D_8004F308 = -1;
        if (D_8004F2FC != 0) {
            D_8004F348 = 1;
        }
        func_80085B20(D_8004F324, 1);
    } else {
        func_80085EEC();
    }
    func_800A31E8();
    if (D_800594D0 != 1) {
        if (D_8005942C != 1) {
            func_80071E58(0x20);
            shade = 0x800000;
            for (i = 0; i < 0x20; i++) {
                func_80077DAC();
                func_800A6408();
                func_8007554C();
                func_80078B5C();
                if (D_800594D0 != 1) {
                    func_800A5600(shade >> 16);
                    shade -= 0x40000;
                    if (shade < 0) {
                        shade = 0;
                    }
                    if (D_800C2684 < 0x22C0) {
                        D_800C2684 += grow;
                    }
                }
            }
        } else {
            func_80071E58(0x20);
        }
    } else {
        for (i = 0; i < 8; i++) {
            func_80077DAC();
            func_800A6408();
            func_8007554C();
            func_80078B5C();
        }
    }
    if (D_800B2078.unk2264 != 0) {
        func_801E7378(0);
    }
    func_800A91F0();
    func_80031FF8();
    func_8003748C();
    func_80077544();
    D_800AFD04 = 0;
}

extern u8 D_800594F8;
extern u8 D_80059508;       /* the battle's encounter kind */
extern u8 D_80065ADC[16];   /* encounter kind weights */
void func_800199CC(s32 mode);
void func_80281204(s32 kind);

/* Count down the random-encounter steps while encounters are possible; on a
 * step whose drawn number (800b22a0) reaches zero, pick an encounter kind by
 * the weights at 80065adc and request battle with its music. The original
 * is an int function (implicit int) that returns no value: its epilogue keeps
 * $v0 live, so no delay slot is filled with a $v0 write. */
s32 func_80079288(void) {
    s32 start[16];
    s32 spare[2]; /* unused in the original; reserves 8 bytes */
    u8 *weights;
    s32 total;
    s32 sum;
    s32 found;
    s32 i;

    if (D_800ADBDC == 0 || D_800ADBE4 == 0 || D_800ADBEC == 0 || D_8004F308 == -1 || D_800B2078.unk2298 == 0
        || D_800B2078.encounter_inhibition == -1 || D_800ADB2C == 1 || D_800ADB04 == 0) {
        return;
    }
    if (--D_800B2078.unk2294 == 0) {
        func_8008E718();
    }
    for (i = 0; i < D_800B2078.unk229C; i++) {
        if (D_800B2078.unk22A0[i] != 0xFFFF) {
            D_800B2078.unk22A0[i]--;
        }
    }
    for (i = 0; i < D_800B2078.unk229C; i++) {
        if (D_800B2078.unk22A0[i] == 0) {
            D_800B2078.unk22A0[i] = 0xFFFF;
            goto draw;
        }
    }
    return;
draw:
    weights = D_80065ADC;
    total = 0;
    for (i = 0; i < 16; i++) {
        total += weights[i];
    }
    sum = 0;
    for (i = 0; i < 16; i++) {
        start[i] = sum;
        sum += weights[i];
    }
    total = (rand() * (total + 1)) >> 15;
    found = 0;
    for (i = 15; i >= 0; i--) {
        if (weights[i] != 0 && start[i] < total) {
            found++;
            break;
        }
    }
    if (found != 0) {
        D_80059508 = i;
        D_800594F8 = 0;
        D_800B2290 = D_800B2078.encounter_music[i];
        if (D_8004F370 == 0) {
            func_800199CC(2);
        }
        D_800ADBDC = 0;
        D_800ADBD0 = 1;
        if (D_800C268C == 0) {
            func_80281204(i);
        }
    }
}

extern s32 D_8004F30C;
extern s32 D_8004F310;
extern s32 D_8004F324;
extern s32 D_800AFC78;
extern s32 D_800B0064;
void func_8001996C(s32 mode);
void func_80019ACC(s32 a0);
void func_8001BB50(void);
void func_800A30FC(void);
s32 func_80085F30(void);
void func_80085FB8(void);

/* Leave the field for another game mode, then run the mode dispatcher:
 * kind 0 selects battle (2) after saving the map and event variable 1 in
 * the game state, kind 1 mode 3 (first stopping the field sound and
 * stream work while 8004f384 is 1), kind 2 mode 4, kind 3 the mode in 800b0064's low bits (bit 7 runs
 * 8001bb50 first). Nothing is selected while 8004f370 is set. */
void func_8007954C(s32 kind) {
    D_8005942C = 0;
    switch (kind) {
    case 0:
        func_800A30FC();
        D_8004F324 = D_800AFC78;
        D_8005A39C->unk2322 = D_800AFC78;
        D_8005A39C->unk2320 = D_8005A39C->vars[1];
        if (D_8004F370 != 0) {
            return;
        }
        func_8001996C(2);
        break;
    case 1:
        if (D_8004F384 == kind) {
            func_8001B66C();
            func_80085FB8();
            func_80028A60(0);
            func_80085F30();
            func_8001B66C();
        }
        if (D_8004F370 != 0) {
            return;
        }
        func_8001996C(3);
        break;
    case 2:
        D_8005A39C->unk2322 = D_8004F324;
        D_8005A39C->unk2320 = D_8005A39C->vars[1];
        if (D_8004F370 != 0) {
            return;
        }
        func_8001996C(4);
        D_8004F30C++;
        break;
    case 3:
        D_8004F310 = 0;
        D_8004F30C = 0;
        if (D_8004F370 != 0) {
            return;
        }
        if (D_800B0064 & 0x80) {
            func_8001BB50();
        }
        func_8001996C(D_800B0064 & 0x7F);
        break;
    }
    func_80019ACC(0);
}

void func_800796F4(void) {
}

/* Switch to the other draw block and put its display and draw environments. */
void func_800796FC(void) {
    D_800ADB08 = (D_800ADB08 + 1) % 2;
    D_800C426C = &D_800B249C[D_800ADB08];
    PutDispEnv(&D_800C426C->disp);
    PutDrawEnv(&D_800C426C->draw);
}

extern DR_MODE D_800AFE24[2]; /* fade draw mode per buffer */
extern RECT D_800AFE4C;       /* fade copy source */
extern TILE D_800AFE54[2];    /* fade tile per buffer */

/* Present the current buffer under a full-screen tile of brightness
 * `level * 4`: link the tile and its draw mode, copy the display area, then
 * put the environments and draw. */
void func_80079784(s32 level) {
    func_80073FE0();
    D_800AFE54[D_800ADB08].r0 = D_800AFE54[D_800ADB08].g0 = D_800AFE54[D_800ADB08].b0 = level * 4;
    addPrim(D_800C426C->ot, &D_800AFE54[D_800ADB08]);
    addPrim(D_800C426C->ot, &D_800AFE24[D_800ADB08]);
    func_800775F8();
    MoveImage(&D_800AFE4C, 0, D_800ADB08 << 8);
    PutDispEnv(&D_800C426C->disp);
    PutDrawEnv(&D_800C426C->draw);
    DrawOTag(&D_800C426C->ot[1]);
}

/* Set the battle-entry flag (80059179): clear only while the controlled
 * actor has neither bit 0x40 nor 0x80 of +14; 800b234c overrides it. */
void func_800798BC(void) {
    if (D_800B2078.unk2268 != 0 && !(D_800AF880.components.descriptors[D_800B2078.controlled].actor->unk014 & 0xC0)) {
        D_80059179 = 0;
    } else {
        D_80059179 = 1;
    }
    if (D_800B2078.battle_override != 0xFF) {
        D_80059179 = D_800B2078.battle_override;
    }
}

/* Move a VRAM rectangle (MoveImage) and wait for it. */
void func_8007995C(s32 w, s32 h, s32 x, s32 y, s32 to_x, s32 to_y) {
    RECT rect;

    rect.w = w;
    rect.h = h;
    rect.x = x;
    rect.y = y;
    MoveImage(&rect, to_x, to_y);
    DrawSync(0);
}

/* Sync, then flush the instruction cache inside a critical section. */
void func_8007999C(void) {
    func_800775F8();
    EnterCriticalSection();
    FlushCache();
    ExitCriticalSection();
}

extern s32 D_8004F350;
extern s16 D_8006BE2C[3];
extern void *D_8005945C;    /* the menu's shared file (1) */
extern u8 D_80059178;
extern u8 D_80059460;       /* menu kind */
extern u32 *D_8005A4AC;     /* the menu's order tables */
extern u32 *D_8005A4B0;
extern s32 D_8004F31C;
extern s32 D_8004F320;
void func_8001C634(void);

/* Run a menu (kind in 800adb64, 0x80 marks a pending event-only one) over
 * the field: fade out, save the 801e module and the VRAM the menu uses,
 * load the menu (file kind + 5, and the shared file 1), run it (8001c634),
 * apply its results (entering a map from a save), then restore VRAM, fade
 * back in and reload the module and the party sprites. */
void func_800799D4(void) {
    RECT rect;
    FieldFileRequest files[4];
    RECT unused; /* unused in the original; reserves 8 bytes */
    u32 end;
    void *module;
    void *source;
    void *menu;
    void *sprites;
    u32 *saved_a;
    u32 *saved_b;
    s32 i;

    module = NULL;
    if (D_800ADB64 == 0x80 && D_800B2078.script_control[0] != 0) {
        return;
    }
    setlen(&D_800AFE54[0], 3);
    setcode(&D_800AFE54[0], 0x60);
    SetSemiTrans(&D_800AFE54[0], 1);
    D_800AFE54[0].w = 0x140;
    D_800AFE54[0].b0 = 0;
    D_800AFE54[0].g0 = 0;
    D_800AFE54[0].r0 = 0;
    D_800AFE54[0].y0 = 0;
    D_800AFE54[0].x0 = 0;
    D_800AFE54[0].h = 0xE0;
    D_800AFE54[1] = D_800AFE54[0];
    SetDrawMode(&D_800AFE24[0], 0, 0, GetTPage(0, 2, 0, 0), NULL);
    SetDrawMode(&D_800AFE24[1], 0, 0, GetTPage(0, 2, 0, 0), NULL);
    func_80077D2C();
    if (D_800B2078.unk2264 != 0) {
        func_80028470(4, 0);
        module = func_80031BDC(func_800288EC(0x6B9), 0);
        source = D_800ADB20;
        memcpy(module, source, func_800288EC(0x6B9));
        func_800320E8(D_800ADB20);
    }
    end = D_800ADB30;
    func_80028470(0x10, 0);
    if (D_8004F370 == 1) {
        menu = func_80031BDC(func_800288EC((D_800ADB64 + 5) & 0x7F), 1);
    } else {
        menu = func_80031BDC((end & 0xFFFFFF) - 0x1C5008, 1);
    }
    files[2].file = 0;
    files[2].destination = NULL;
    files[3].file = 0;
    files[3].destination = NULL;
    files[0].file = 1;
    files[0].destination = D_8005945C = func_80031BDC(func_800288EC(1), 1);
    files[1].destination = menu;
    files[1].file = (D_800ADB64 & 0x7F) + 5;
    if ((D_800ADB64 & 0x7F) == 5 && D_8004F370 == 0) {
        files[2].file = 0xC;
        files[2].destination = (void *)0x1DC000;
    }
    func_80028A60(0);
    func_80029AFC(files, 0, 0);
    func_80028470(4, 0);
    rect.w = 0x40;
    rect.h = 0x20;
    for (i = 0; i < 6; i++) {
        rect.x = D_800ADCB0[i * 2];
        rect.y = D_800ADCB0[i * 2 + 1];
        MoveImage(&rect, D_800ADCC8[i * 2], D_800ADCC8[i * 2 + 1]);
        DrawSync(0);
    }
    func_8007995C(0x40, 0x100, 0x3C0, 0x100, 0x300, 0);
    func_8007995C(0x40, 0x100, 0x2C0, 0x100, 0x280, 0);
    D_800AFE4C.x = 0x2C0;
    D_800AFE4C.y = 0x100;
    D_800AFE4C.w = 0x140;
    D_800AFE4C.h = 0xE0;
    func_800A4748();
    MoveImage(&D_800AFE4C, 0, 0x100);
    DrawSync(0);
    for (i = 0; i < 0x20; i++) {
        func_80079784(i);
    }
    func_800775F8();
    D_800AFE4C.x = 0;
    D_800AFE4C.y = 0;
    func_800796FC();
    func_8003748C();
    MoveImage(&D_800AFE4C, 0, 0xE0);
    func_800775F8();
    func_80028A60(0);
    D_800594D0 = 0;
    D_80059178 = 0;
    D_80059460 = D_800ADB64 & 0x7F;
    for (i = 0; i < 3; i++) {
        D_8006BE2C[i] = D_8005A39C->unk22B1[i];
    }
    func_800798BC();
    D_8005A4AC = D_800B249C[0].ot;
    D_8005A4B0 = D_800B249C[1].ot;
    func_8007999C();
    func_8001C634();
    func_8007999C();
    D_80050100 = 2;
    if (D_800594D0 == 0 && (D_800ADB64 & 0x7F) == 2) {
        D_800B02C8 = 1;
        func_800A3074(0x46, 0);
        func_800A3074(4, 4);
        D_8004F34C = 4;
        D_8005A39C->unk2320 = 0;
        D_8005A39C->vars[1] = 0;
        D_8005A39C->unk231A = 4;
    }
    if (D_800594D0 == 2) {
        D_800B02C8 = 1;
        func_800A3074(0x46, 2);
        func_800A3074(4, D_8005A39C->unk231A & 0x3FFF);
        if ((D_8005A39C->unk231A & 0x3FFF) < 0x400) {
            D_8005A39C->unk2320 = D_8005A39C->vars[0x2A];
        }
    }
    func_800775F8();
    D_800AFE4C.x = 0;
    D_800AFE4C.y = 0xE0;
    D_800AFE4C.w = 0x140;
    D_800AFE4C.h = 0xE0;
    MoveImage(&D_800AFE4C, 0x140, 0);
    func_800775F8();
    D_800AFE4C.x = 0x140;
    D_800AFE4C.y = 0;
    MoveImage(&D_800AFE4C, 0, 0);
    MoveImage(&D_800AFE4C, 0, 0x100);
    func_800775F8();
    PutDispEnv(&D_800C426C->disp);
    PutDrawEnv(&D_800C426C->draw);
    D_800AFE4C.x = 0x2C0;
    D_800AFE4C.y = 0x100;
    func_80079784(0x1F);
    func_80079784(0x1F);
    rect.w = 0x40;
    rect.h = 0x100;
    rect.x = 0x300;
    rect.y = 0;
    saved_a = func_80031BDC(0x8000, 1);
    StoreImage(&rect, saved_a);
    DrawSync(0);
    rect.w = 0x40;
    rect.h = 0x100;
    rect.x = 0x280;
    rect.y = 0;
    saved_b = func_80031BDC(0x8000, 1);
    StoreImage(&rect, saved_b);
    DrawSync(0);
    for (i = 0; i < 6; i++) {
        rect.w = 0x40;
        rect.h = 0x20;
        rect.x = D_800ADCC8[i * 2];
        rect.y = D_800ADCC8[i * 2 + 1];
        MoveImage(&rect, D_800ADCB0[i * 2], D_800ADCB0[i * 2 + 1]);
        DrawSync(0);
    }
    func_80028470(4, 0);
    func_80032498(8, 0);
    D_800ADB60 = 0;
    func_80070488();
    if (D_800AFE84 != 0) {
        for (i = 0x20; i < 0x3F; i++) {
            func_80079784(i);
        }
        D_800ADB50 = 1;
    } else {
        for (i = 0x1F; i >= 0; i--) {
            func_80079784(i);
        }
        func_80079784(0);
        D_800ADB50 = 0;
    }
    func_80070508();
    func_800775F8();
    rect.w = 0x40;
    rect.h = 0x100;
    rect.x = 0x2C0;
    rect.y = 0x100;
    LoadImage(&rect, saved_b);
    DrawSync(0);
    rect.x = 0x3C0;
    LoadImage(&rect, saved_a);
    DrawSync(0);
    func_800320E8(saved_b);
    func_800320E8(saved_a);
    func_800320E8(menu);
    func_80028470(4, 0);
    if (D_800B2078.unk2264 != 0) {
        if (D_8004F370 == 0) {
            D_800ADB20 = func_80031BDC((end & 0xFFFFFF) - 0x1DC008, 1);
        } else {
            D_800ADB20 = func_80031BDC(func_800288EC(0x6B9), 1);
        }
        source = D_800ADB20;
        memcpy(source, module, func_800288EC(0x6B9));
        func_800320E8(module);
    }
    SetGeomOffset(0xA0, 0x70);
    SetGeomScreen(D_800AF880.projection);
    func_80077C88();
    if (D_800ADB64 == 1) {
        D_8006FABC[2] = 0xFF;
        D_8006FABC[1] = 0xFF;
        D_8006FABC[0] = 0xFF;
        D_8004F320 = 0;
        D_8004F31C = 0;
        func_8001B044();
        func_8001B3A8();
        func_800775F8();
        D_800ADBEC = 0;
        D_800ADB05 = 1;
    } else {
        for (i = 0; i < 3; i++) {
            if (D_8006FABC[i] != 0xFF) {
                sprites = func_80031BDC(func_800288EC(D_8006FABC[i] + 5), 1);
                func_800295D8(D_8006FABC[i] + 5, sprites, 0, 0x80);
                func_80028A60(0);
                func_80032EB4(sprites, D_8005A414[i]);
                func_800320E8(sprites);
            }
        }
        func_800A2488();
        func_800775F8();
    }
    D_800ADB64 = 0xFF;
    func_80077544();
    D_8004F350 = 0;
}
