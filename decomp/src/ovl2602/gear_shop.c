/*
 * Gear screen and shop code (801ce1d0 onwards): the model and parts panel,
 * member switching, stock and sale lists, fitting and refuelling. These
 * callers pass quad coordinates as words, while the shared screen unit's
 * local calls see the helper's narrow definition.
 */
#include "menu_card.h"

void func_801C7604();

/* Draw the two second-marker sprites and set their four quads. */
void func_801CE1D0(void) {
    POLY_FT4 *poly;
    MarkerQuads *marks;
    s32 i;

    for (i = 0; i < 2; i++) {
        func_8002675C(D_800625A0->sprite_sheet, i + 0x164, &D_800625A0->marks_b->packets[i * 4],
                      D_800625A0->buffer, D_801D6FD0[i], 0x64, 0x1000);
    }
    for (i = 0; i < 4; i++) {
        marks = D_800625A0->marks_b;
        poly = &marks->packets[i * 2 + D_800625A0->buffer];
        func_801C7604(&D_800625A0->marks_b->quads[i * 4], poly->x0, poly->y0, poly->x1 - poly->x0,
                      poly->y3 - poly->y0);
    }
    D_800625A0->marks_b->buffer = D_800625A0->buffer;
}

/* Hide the second marker block, let a frame pass, and release it. */
void func_801CE2E8(void) {
    D_800625A0->flags->marks_b_shown = 0;
    func_801CC1C4();
    func_800320E8(D_800625A0->marks_b);
}

/* Draw the shop's detail packets: member bars, portraits, headings, list rows, labels, numbers and prices. */
void func_801CE32C(void) {
    s32 i;

    if (D_800625A0->flags->unk5A != 0) {
        for (i = 0; i < 9; i++) {
            if (D_800625A0->details->bar_shown[i] != 0) {
                AddPrim(&D_800625A0->draw_env->ot[4],
                        &D_800625A0->details->bar_upper[i * 2 + D_800625A0->buffer]);
                AddPrim(&D_800625A0->draw_env->ot[4],
                        &D_800625A0->details->bar_lower[i * 2 + D_800625A0->buffer]);
            }
        }
        func_801C94CC(D_800625A0->details->heading_count, D_800625A0->details->heading,
                      D_800625A0->details->heading_buffer);
        func_801C94CC(D_800625A0->details->group2D0_count, D_800625A0->details->group2D0,
                      D_800625A0->details->group2D0_buffer);
        func_801C94CC(D_800625A0->details->members_count, D_800625A0->details->members,
                      D_800625A0->details->members_buffer);
        for (i = 0; i < 8; i++) {
            if (D_800625A0->details->name_shown[i] != 0) {
                func_801C93B0(1, D_800625A0->details->names_a[i].quad, D_800625A0->details->names_a[i].poly,
                              D_800625A0->details->names_a[i].buffer);
                func_801C93B0(1, D_800625A0->details->names_b[i].quad, D_800625A0->details->names_b[i].poly,
                              D_800625A0->details->names_b[i].buffer);
            }
        }
        if (D_800625A0->details->label4530_shown != 0) {
            func_801C93B0(1, D_800625A0->details->label4530.quad, D_800625A0->details->label4530.poly,
                          D_800625A0->details->label4530.buffer);
        }
        if (D_800625A0->details->label4430_shown != 0) {
            func_801C93B0(1, D_800625A0->details->label4430.quad, D_800625A0->details->label4430.poly,
                          D_800625A0->details->label4430.buffer);
        }
        if (D_800625A0->details->label44B0_shown != 0) {
            func_801C93B0(1, D_800625A0->details->label44B0.quad, D_800625A0->details->label44B0.poly,
                          D_800625A0->details->label44B0.buffer);
        }
        if (D_800625A0->details->label45B0_shown != 0) {
            func_801C93B0(1, D_800625A0->details->label45B0.quad, D_800625A0->details->label45B0.poly,
                          D_800625A0->details->label45B0.buffer);
        }
        if (D_800625A0->details->digits_shown != 0) {
            AddPrim(&D_800625A0->draw_env->ot[4], &D_800625A0->details->frame[D_800625A0->buffer]);
            func_801C94CC(D_800625A0->details->digits1_count, D_800625A0->details->digits1,
                          D_800625A0->details->digits1_buffer);
            func_801C94CC(D_800625A0->details->digits2_count, D_800625A0->details->digits2,
                          D_800625A0->details->digits2_buffer);
            func_801C94CC(D_800625A0->details->digits3_count, D_800625A0->details->digits3,
                          D_800625A0->details->digits3_buffer);
            if (D_800625A0->details->digits4_shown != 0) {
                func_801C94CC(D_800625A0->details->digits4_count, D_800625A0->details->digits4,
                              D_800625A0->details->digits4_buffer);
            }
        }
        for (i = 0; i < 8; i++) {
            func_801C94CC(D_800625A0->details->row_count[i], D_800625A0->details->rows[i],
                          D_800625A0->details->row_buffer[i]);
        }
        for (i = 0; i < 9; i++) {
            func_801C94CC(D_800625A0->details->cells_a_count[i], D_800625A0->details->cells_a[i],
                          D_800625A0->details->cells_a_buffer[i]);
            func_801C94CC(D_800625A0->details->cells_b_count[i], D_800625A0->details->cells_b[i],
                          D_800625A0->details->cells_b_buffer[i]);
        }
    }
    if (D_800625A0->flags->unk5B == 1) {
        func_801C94CC(D_800625A0->details->group1220_count, D_800625A0->details->group1220,
                      D_800625A0->details->group1220_buffer);
    }
    if (D_800625A0->flags->price_shown != 0) {
        func_801C94CC(D_800625A0->details->price_count, D_800625A0->details->price,
                      D_800625A0->details->price_buffer);
    }
}

/* Draw the separately loaded model when shown. */
void func_801CE7E0(void) {
    if (D_800625A0->flags->model_shown != 0) {
        func_801E7D14(&D_800625A0->model_matrix, &D_800625A0->model_light, D_800625A0->draw_env->ot_big, D_800625A0->buffer);
    }
}

/* Draw the gear screen's animated sprites, then its backdrop, frame and part pictures when shown. */
void func_801CE82C(void) {
    s32 i;

    if (D_800625A0->flags->gear_shown != 0) {
        if (D_800625A0->unk454->flicker_shown != 0) {
            func_801C94CC(D_800625A0->unk454->flicker_count, D_800625A0->unk454->flicker[0],
                          D_800625A0->unk454->flicker_buffer);
            func_801C94CC(D_800625A0->unk454->flicker_count, D_800625A0->unk454->flicker[1],
                          D_800625A0->unk454->flicker_buffer);
            func_801C94CC(D_800625A0->unk454->flicker_count, D_800625A0->unk454->flicker[2],
                          D_800625A0->unk454->flicker_buffer);
        }
        for (i = 0; i < 2; i++) {
            if (D_800625A0->unk454->lamp_state[i] != 0) {
                func_801C94CC(D_800625A0->unk454->lamp_count[i], &D_800625A0->unk454->lamps[i * 22],
                              D_800625A0->unk454->lamp_buffer[i]);
            }
        }
        if (D_800625A0->unk454->lamp_state[2] != 0) {
            func_801C94CC(D_800625A0->unk454->lamp_count[2], D_800625A0->unk454->indicator,
                          D_800625A0->unk454->lamp_buffer[2]);
        }
    }
    if (D_800625A0->flags->gear_parts_shown != 0) {
        func_801C94CC(1, D_800625A0->unk454->backdrop, D_800625A0->buffer);
        func_801C94CC(0xE, D_800625A0->unk454->frame, D_800625A0->unk454->parts_buffer);
        func_801C94CC(D_800625A0->unk454->part_count[0], D_800625A0->unk454->parts[0],
                      D_800625A0->unk454->parts_buffer);
        func_801C94CC(D_800625A0->unk454->part_count[1], D_800625A0->unk454->parts[1],
                      D_800625A0->unk454->parts_buffer);
        func_801C94CC(D_800625A0->unk454->part_count[2], D_800625A0->unk454->parts[2],
                      D_800625A0->unk454->parts_buffer);
        func_801C94CC(D_800625A0->unk454->part_count[3], D_800625A0->unk454->parts[3],
                      D_800625A0->unk454->parts_buffer);
        func_801C94CC(D_800625A0->unk454->part_count[4], D_800625A0->unk454->parts[4],
                      D_800625A0->unk454->parts_buffer);
    }
}

/* Animate the two lamps: pick their position (from the cursor while opening, now and then at random while idle), draw the frame's sprites and step the frame. */
void func_801CEA68(void) {
    u16 at[2][4];
    u8 moved;
    s32 i;
    s32 j;
    u16 id;

    for (i = 0; i < 2; i++) {
        moved = 0;
        if (D_800625A0->unk454->lamp_state[i] == 0) {
            continue;
        }
        switch (D_800625A0->unk454->lamp_state[i]) {
        case 1:
            at[0][i] = D_801D7040[i];
            at[1][i] = D_801D7044[i * 6 + D_801D7030[D_800625A0->top_cursor * 4 + D_800625A0->list_cursor]];
            moved = 1;
            break;
        case 2:
            if (func_8001BD40(0, 0xFF) < 0x10) {
                at[0][i] = D_801D7040[i];
                at[1][i] = D_801D7044[i * 6 + func_8001BD40(0, 5)];
                moved = 1;
            }
            break;
        }
        if (moved) {
            D_800625A0->unk454->lamp_x[i] = at[0][i];
            D_800625A0->unk454->lamp_y[i] = at[1][i];
        }
        if (D_800625A0->unk454->lamp_timer[i] != 0) {
            D_800625A0->unk454->lamp_timer[i]--;
            continue;
        }
        D_800625A0->unk454->lamp_count[i] = 0;
        for (j = 0; j < 4; j++) {
            id = D_801D6FE0[i * 20 + (D_800625A0->unk454->lamp_frame[i] * 4 + j)];
            if (id != 0xFFFF) {
                D_800625A0->unk454->lamp_count[i] +=
                    func_8002675C(D_800625A0->sprite_sheet, id,
                                  &D_800625A0->unk454->lamps[i * 22 + D_800625A0->unk454->lamp_count[i] * 2],
                                  D_800625A0->buffer, D_800625A0->unk454->lamp_x[i],
                                  D_800625A0->unk454->lamp_y[i], 0x1000);
            }
        }
        D_800625A0->unk454->lamp_buffer[i] = D_800625A0->buffer;
        switch (D_800625A0->unk454->lamp_state[i]) {
        case 1:
            if (++D_800625A0->unk454->lamp_frame[i] == 4) {
                D_800625A0->unk454->lamp_state[i] = 2;
            }
            D_800625A0->unk454->lamp_timer[i] = 2;
            break;
        case 2:
            if (++D_800625A0->unk454->lamp_frame[i] >= 5) {
                D_800625A0->unk454->lamp_frame[i] = 3;
            }
            D_800625A0->unk454->lamp_timer[i] = 8;
            break;
        case 3:
            if (--D_800625A0->unk454->lamp_frame[i] < 0) {
                D_800625A0->unk454->lamp_state[i] = 0;
                D_800625A0->unk454->flicker_shown = 0;
            }
            D_800625A0->unk454->lamp_timer[i] = 2;
            break;
        }
    }
}

/* Animate the indicator: its position (from the cursor while opening, now and then at random while idle), its sprite, and its open-idle-close frame steps. */
void func_801CEEA8(void) {
    u8 moved;
    u16 x;
    u16 y;
    s32 index;

    if (D_800625A0->unk454->lamp_state[2] != 0) {
        if (D_800625A0->unk454->lamp_timer[2] != 0) {
            D_800625A0->unk454->lamp_timer[2]--;
            return;
        }
        moved = 0;
        switch (D_800625A0->unk454->lamp_state[2]) {
        case 1:
            index = D_801D7030[D_800625A0->top_cursor * 4 + D_800625A0->list_cursor];
            x = D_801D705C[index];
            moved = 1;
            y = D_801D7068[index];
            break;
        case 2:
            if (func_8001BD40(0, 0xFF) < 4) {
                x = D_801D705C[func_8001BD40(0, 5)];
                y = D_801D7068[func_8001BD40(0, 5)];
                moved = 1;
            }
            break;
        }
        if (moved) {
            D_800625A0->unk454->indicator_x = x;
            D_800625A0->unk454->indicator_y = y;
        }
        D_800625A0->unk454->lamp_count[2] =
            func_8002675C(D_800625A0->sprite_sheet, D_800625A0->unk454->indicator_frame + 0x172,
                          D_800625A0->unk454->indicator, D_800625A0->buffer, D_800625A0->unk454->indicator_x,
                          D_800625A0->unk454->indicator_y, 0x1000);
        D_800625A0->unk454->lamp_buffer[2] = D_800625A0->buffer;
        D_800625A0->unk454->lamp_timer[2] = 1;
        switch (D_800625A0->unk454->lamp_state[2]) {
        case 1:
            if (++D_800625A0->unk454->indicator_frame >= 3) {
                D_800625A0->unk454->lamp_state[2] = 2;
            }
            break;
        case 2:
            if (++D_800625A0->unk454->indicator_frame >= 11) {
                D_800625A0->unk454->indicator_frame = 3;
            }
            break;
        case 3:
            D_800625A0->unk454->indicator_frame = 2;
            D_800625A0->unk454->lamp_state[2] = 4;
            break;
        case 4:
            if (--D_800625A0->unk454->indicator_frame < 0) {
                D_800625A0->unk454->lamp_state[2] = 0;
            }
            break;
        }
    }
}

/* Animate the flicker every fifth frame: now and then move it to a random place, draw its three sprites (a diagonal row) and toggle their frame. */
void func_801CF184(void) {
    u16 x;
    u16 y;
    s32 i;

    if (D_800625A0->unk454->flicker_shown != 0) {
        if (D_800625A0->unk454->flicker_timer != 0) {
            D_800625A0->unk454->flicker_timer--;
            return;
        }
        if (func_8001BD40(0, 0xFF) < 8) {
            x = D_801D7074[func_8001BD40(0, 5)];
            y = D_801D7080[func_8001BD40(0, 5)];
            D_800625A0->unk454->flicker_x = x;
            D_800625A0->unk454->flicker_y = y;
        }
        for (i = 0; i < 3; i++) {
            D_800625A0->unk454->flicker_count =
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->unk454->flicker_frame + 0x167,
                              D_800625A0->unk454->flicker[i], D_800625A0->buffer,
                              D_800625A0->unk454->flicker_x + i * 8, D_800625A0->unk454->flicker_y + i * 10,
                              0x1000);
        }
        D_800625A0->unk454->flicker_frame ^= 1;
        D_800625A0->unk454->flicker_buffer = D_800625A0->buffer;
        D_800625A0->unk454->flicker_timer = 4;
    }
}

/* Build the gear screen's packets when shown. */
void func_801CF33C(void) {
    if (D_800625A0->flags->gear_shown != 0) {
        func_801CEA68();
        func_801CEEA8();
        func_801CF184();
    }
}

/* Render name `index` of the name table into VRAM at (180h, 48h). */
void func_801CF38C(u8 index) {
    RECT rect;

    D_801D9088 = func_80031BDC(0x3F6, 0);
    bzero(D_801D9088, 0x3F6);
    func_80034EAC(D_8006D634.names[index], D_801D9088, 0x24, 0);
    rect.x = 0x180;
    rect.y = 0x48;
    rect.w = 0x28;
    rect.h = 13;
    LoadImage(&rect, D_801D9088);
    DrawSync(0);
    func_800320E8(D_801D9088);
}

/* Build the gear parts panel: its fourteen frame sprites, five of the gear's values in decimal, and the gear's name. */
void func_801CF448(void) {
    s32 i;
    s32 j;

    for (j = 0; j < 14; j++) {
        func_8002675C(D_800625A0->sprite_sheet, D_801D708C[j], &D_800625A0->unk454->frame[j * 2],
                      D_800625A0->buffer, D_801D70A8[j], D_801D70C4[j], 0x1000);
    }
    func_801C5298(D_8006DFAC[D_801D9084].hp);
    D_800625A0->unk454->part_count[0] = 0;
    for (i = 0; i < 5; i++) {
        if (D_800625A0->digits[i + 4] != 0xFF) {
            D_800625A0->unk454->part_count[0] +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[i + 4],
                              &D_800625A0->unk454->parts[0][D_800625A0->unk454->part_count[0] * 2],
                              D_800625A0->buffer, D_801D70E0 + i * 8, D_801D70E2, 0x1000);
        }
    }
    func_801C5298(D_8006DFAC[D_801D9084].max_hp);
    D_800625A0->unk454->part_count[1] = 0;
    for (i = 0, j = 0; i < 5; i++) {
        if (D_800625A0->digits[i + 4] != 0xFF) {
            D_800625A0->unk454->part_count[1] +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[i + 4],
                              &D_800625A0->unk454->parts[1][D_800625A0->unk454->part_count[1] * 2],
                              D_800625A0->buffer, D_801D70E4 + j * 8, D_801D70E6, 0x1000);
            j++;
        }
    }
    func_801C5298(D_8006DFAC[D_801D9084].fuel);
    D_800625A0->unk454->part_count[2] = 0;
    for (i = 0; i < 4; i++) {
        if (D_800625A0->digits[i + 5] != 0xFF) {
            D_800625A0->unk454->part_count[2] +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[i + 5],
                              &D_800625A0->unk454->parts[2][D_800625A0->unk454->part_count[2] * 2],
                              D_800625A0->buffer, D_801D70E8 + i * 8, D_801D70EA, 0x1000);
        }
    }
    func_801C5298(D_8006DFAC[D_801D9084].max_fuel);
    D_800625A0->unk454->part_count[3] = 0;
    for (i = 0, j = 0; i < 4; i++) {
        if (D_800625A0->digits[i + 5] != 0xFF) {
            D_800625A0->unk454->part_count[3] +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[i + 5],
                              &D_800625A0->unk454->parts[3][D_800625A0->unk454->part_count[3] * 2],
                              D_800625A0->buffer, D_801D70EC + j * 8, D_801D70EE, 0x1000);
            j++;
        }
    }
    func_801C5298(D_8006DFAC[D_801D9084].unk68);
    D_800625A0->unk454->part_count[4] = 0;
    for (i = 0; i < 5; i++) {
        if (D_800625A0->digits[i + 4] != 0xFF) {
            D_800625A0->unk454->part_count[4] +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[i + 4],
                              &D_800625A0->unk454->parts[4][D_800625A0->unk454->part_count[4] * 2],
                              D_800625A0->buffer, D_801D70F0 + i * 8, D_801D70F2, 0x1000);
        }
    }
    func_801CF38C(D_801D9084 + 0xB);
    D_800625A0->unk454->parts_buffer = D_800625A0->buffer;
}

/* Load model `model`'s two files (ids from the model table) into part block `slot`. */
void func_801CF9BC(u8 model, u8 slot) {
    func_80028470(4, 0);
    D_800625A0->model_parts[slot]->data0 = func_80031BDC(func_800288EC(D_801D6D7C[model]), 0);
    func_800295D8(D_801D6D7C[model], D_800625A0->model_parts[slot]->data0, 0, 0x80);
    func_80028A60(0);
    D_800625A0->model_parts[slot]->data1 = func_80031BDC(func_800288EC(D_801D6D7C[model] + 1), 0);
    func_800295D8(D_801D6D7C[model] + 1, D_800625A0->model_parts[slot]->data1, 0, 0x80);
    func_80028A60(0);
    func_80028470(0x10, 0);
}

/*
 * Load gear `gear`'s model into slot `slot` (with the model code outside this
 * overlay), move the camera back, and show the parts panel. Nonmatching: the
 * original zero-extends `slot` twice (a second saved register).
 */
#ifdef NON_MATCHING
void func_801CFAB8(u8 slot, u8 gear) {
    u8 variant;

    func_801E742C(slot, 0, D_800625A0->model_parts[slot]->data0, D_800625A0->model_parts[slot]->data1,
                  (slot << 6) + 0x200, 0, 0, slot + 0x1C0, D_800625A0->model_parts[slot]->position);
    D_801E8670[slot]->unk60 = D_801D6DB4[gear];
    D_801E8670[slot]->unk1C = D_801D6DD8[gear];
    D_801E8670[1]->view->distance -= 0x400;
    D_80050100 = 0;
    D_801E8670[1]->view->unk54 -= 0x20;
    variant = 0;
    if (gear != 0xFF) {
        variant = D_801D6DA0[gear];
    }
    func_801E8330(slot, 0, variant);
    func_800320E8(D_800625A0->model_parts[slot]->data1);
    D_800625A0->model_parts[slot]->unk12 = 1;
    func_801CF448();
    D_800625A0->flags->gear_parts_shown = 1;
}
#else
INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/gear_shop", func_801CFAB8);
#endif

/*
 * Swing the camera to the edited gear's view for the current command, open
 * the lamps, indicator and flicker, and wait until the lamps and indicator
 * are open. Nonmatching: the first two stores are scheduled in the other order.
 */
#ifdef NON_MATCHING
void func_801CFC60(void) {
    D_801D9050.from[0] = D_801D9050.to[0];
    D_801D9050.from[1] = D_801D9050.to[1];
    D_801D9050.from[2] = D_801D9050.to[2];
    D_801D9050.to[0] = D_801D6DFC[D_801D9084];
    D_801D9050.to[1] = D_801D6E18[(D_801D9084 * 3 + D_800625A0->top_cursor) * 4 + D_800625A0->list_cursor];
    D_801D9050.to[2] = D_801D6FB0[D_800625A0->top_cursor * 4 + D_800625A0->list_cursor];
    func_801CB690();
    D_800625A0->view_motion = 7;
    D_800625A0->unk454->flicker_shown = 1;
    D_800625A0->unk454->lamp_state[0] = 1;
    D_800625A0->unk454->lamp_state[1] = 1;
    D_800625A0->unk454->lamp_state[2] = 1;
    D_800625A0->unk454->flicker_timer = 0;
    D_800625A0->unk454->lamp_timer[0] = 0;
    D_800625A0->unk454->lamp_timer[1] = 0;
    D_800625A0->unk454->lamp_timer[2] = 0;
    D_800625A0->unk454->flicker_frame = 0;
    D_800625A0->unk454->lamp_frame[0] = 0;
    D_800625A0->unk454->lamp_frame[1] = 0;
    D_800625A0->unk454->indicator_frame = 0;
    D_800625A0->unk454->flicker_x = 0x18;
    D_800625A0->unk454->flicker_y = 0x6E;
    D_800625A0->flags->gear_shown = 1;
    while (D_800625A0->unk454->lamp_state[0] != 2) {
        func_801CC1C4();
    }
    while (D_800625A0->unk454->lamp_state[2] != 2) {
        func_801CC1C4();
    }
}
#else
INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/gear_shop", func_801CFC60);
#endif

/* Show the gear screen: close its lamps and indicator, swing the camera and start its motion (7). */
void func_801CFF18(void) {
    D_800625A0->unk454->lamp_state[0] = 3;
    D_800625A0->unk454->lamp_state[1] = 3;
    D_800625A0->unk454->lamp_state[2] = 3;
    D_800625A0->unk454->flicker_timer = 0;
    D_800625A0->unk454->lamp_timer[0] = 0;
    D_800625A0->unk454->lamp_timer[1] = 0;
    D_800625A0->unk454->lamp_timer[2] = 0;
    D_800625A0->flags->gear_shown = 1;
    D_801D9050.from[0] = D_801D9050.to[0];
    D_801D9050.from[1] = D_801D9050.to[1];
    D_801D9050.from[2] = D_801D9050.to[2];
    D_801D9050.to[0] = 0x400;
    D_801D9050.to[1] = 0;
    D_801D9050.to[2] = -0x400;
    func_801CB690();
    D_800625A0->view_motion = 7;
}

/* Tint `count` packet pairs of this buffer red (0) or blue (1). */
void func_801D0054(s32 count, POLY_FT4 *packets, u8 color) {
    s32 i;

    for (i = 0; i < count; i++) {
        SetShadeTex(&packets[i * 2 + D_800625A0->buffer], 0);
        switch (color) {
        case 0:
            (packets + (i * 2 + D_800625A0->buffer))->r0 = 0x80;
            (packets + (i * 2 + D_800625A0->buffer))->g0 = 0x40;
            (packets + (i * 2 + D_800625A0->buffer))->b0 = 0x40;
            break;
        case 1:
            (packets + (i * 2 + D_800625A0->buffer))->r0 = 0x40;
            (packets + (i * 2 + D_800625A0->buffer))->g0 = 0x40;
            (packets + (i * 2 + D_800625A0->buffer))->b0 = 0x80;
            break;
        }
    }
}

/* Reveal the available party members' portraits one member per frame. */
void func_801D0220(void) {
    s32 step;
    s32 shown;
    s32 i;

    D_800625A0->flags->unk5A = 1;
    for (step = 1; step < 12; step++) {
        shown = 0;
        D_800625A0->details->members_count = 0;
        for (i = 0; i < step; i++) {
            if (D_800625A0->member_present[i] != 0) {
                D_800625A0->details->members_count +=
                    func_8002675C(D_800625A0->sprite_sheet, i + 0x14E, &D_800625A0->details->members[shown * 2],
                                  D_800625A0->buffer, D_801D6C44[shown], 0xA6, 0x1000);
                shown++;
            }
        }
        D_800625A0->details->members_buffer = D_800625A0->buffer;
        func_801CC1C4();
    }
}

/* Count the available members 0-10 into D_801D6FD8. */
void func_801D0348(void) {
    s32 i;

    for (i = 0; i < 11; i++) {
        if (D_800625A0->member_present[i] != 0) {
            D_801D6FD8++;
        }
    }
}

/* Switch the gear screen to the previous (`back`) or next available member and load its gear. */
void func_801D0398(u8 back) {
    s32 next;
    s32 member;

    if (D_800625A0->view_motion == 0 && D_801D6FD8 >= 2) {
        next = D_801D6FDC;
        if (!back) {
            if (++next >= D_801D6FD8) {
                next = 0;
            }
        } else {
            if (--next < 0) {
                next = D_801D6FD8 - 1;
            }
        }
        D_801D6FDC = next;
        next++;
        member = -1;
        while (next != 0) {
            member++;
            if (D_800625A0->member_present[member] != 0) {
                next--;
            }
        }
        func_801CC1C4();
        D_800625A0->model_parts[1]->unk12 = 0;
        func_801E8030(1);
        func_801CC1C4();
        func_801CF9BC(D_8006D8A0[member].unkA0, 1);
        D_801D9084 = D_8006D8A0[member].unkA0;
        func_801CFAB8(1, D_8006D8A0[member].unkA0);
        func_801CC1C4();
    }
}

/* Draw heading set `set` (four sprites). */
void func_801D04E8(u8 set) {
    s32 i;

    D_800625A0->details->heading_count = 0;
    for (i = 0; i < 4; i++) {
        D_800625A0->details->heading_count +=
            func_8002675C(D_800625A0->sprite_sheet, D_801D6D08[set * 4 + i],
                          D_800625A0->details->heading + D_800625A0->details->heading_count * 2,
                          D_800625A0->buffer, D_801D6D14[set * 4 + i], D_801D6D3C[set * 4 + i], 0x1000);
    }
    D_800625A0->details->heading_buffer = D_800625A0->buffer;
}

/* Draw the two alternative heading sprites. */
void func_801D05EC(void) {
    s32 i;

    D_800625A0->details->heading_count = 0;
    for (i = 0; i < 2; i++) {
        D_800625A0->details->heading_count +=
            func_8002675C(D_800625A0->sprite_sheet, D_801D6D10[i],
                          D_800625A0->details->heading + D_800625A0->details->heading_count * 2,
                          D_800625A0->buffer, D_801D6D34[i], D_801D6D5C[i], 0x1000);
    }
    D_800625A0->details->heading_buffer = D_800625A0->buffer;
}

/* Show the number panel: its two frame lines and three numbers, plus a fourth when `lower` (shifted left and up by `lower`). */
void func_801D06D8(u32 first, u32 second, u32 third, u32 fourth, u8 lower) {
    s32 i;

    for (i = 0; i < 2; i++) {
        SetLineF2(&D_800625A0->details->frame[i]);
        (D_800625A0->details->frame + i)->r0 = 0xFF;
        (D_800625A0->details->frame + i)->g0 = 0xFF;
        (D_800625A0->details->frame + i)->b0 = 0xFF;
        (D_800625A0->details->frame + i)->x0 = D_801D6D6C - 8 - lower * 16;
        (D_800625A0->details->frame + i)->y0 = D_801D6D70 + 9 - lower * 8;
        (D_800625A0->details->frame + i)->x1 = D_801D6D6C + 0x4E - lower * 16;
        (D_800625A0->details->frame + i)->y1 = D_801D6D70 + 9 - lower * 8;
    }
    func_801C5298(first);
    D_800625A0->details->digits1_count = 0;
    for (i = 0; i < 9; i++) {
        if (D_800625A0->digits[i] != 0xFF) {
            D_800625A0->details->digits1_count +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[i],
                              &D_800625A0->details->digits1[D_800625A0->details->digits1_count * 2],
                              D_800625A0->buffer, D_801D6D64 + i * 8 - lower * 16, D_801D6D68 - lower * 16,
                              0x1000);
        }
    }
    D_800625A0->details->digits1_buffer = D_800625A0->buffer;
    func_801C5298(second);
    D_800625A0->details->digits2_count = 0;
    for (i = 0; i < 9; i++) {
        if (D_800625A0->digits[i] != 0xFF) {
            D_800625A0->details->digits2_count +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[i],
                              &D_800625A0->details->digits2[D_800625A0->details->digits2_count * 2],
                              D_800625A0->buffer, D_801D6D6C + i * 8 - lower * 16, D_801D6D70 - lower * 16,
                              0x1000);
        }
    }
    D_800625A0->details->digits2_buffer = D_800625A0->buffer;
    func_801C5298(third);
    D_800625A0->details->digits3_count = 0;
    for (i = 0; i < 9; i++) {
        if (D_800625A0->digits[i] != 0xFF) {
            D_800625A0->details->digits3_count +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[i],
                              &D_800625A0->details->digits3[D_800625A0->details->digits3_count * 2],
                              D_800625A0->buffer, D_801D6D74 + i * 8 - lower * 16, D_801D6D78 - lower * 8,
                              0x1000);
        }
    }
    D_800625A0->details->digits3_buffer = D_800625A0->buffer;
    D_800625A0->details->digits4_count = 0;
    D_800625A0->details->digits4_shown = 0;
    if (lower) {
        func_801C5298(fourth);
        for (i = 0; i < 9; i++) {
            if (D_800625A0->digits[i] != 0xFF) {
                D_800625A0->details->digits4_count +=
                    func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[i],
                                  &D_800625A0->details->digits4[D_800625A0->details->digits4_count * 2],
                                  D_800625A0->buffer, D_801D6D6C + i * 8 - lower * 16, D_801D6D70 - lower * 8,
                                  0x1000);
            }
        }
        D_800625A0->details->digits4_buffer = D_800625A0->buffer;
        D_800625A0->details->digits4_shown = 1;
    }
    D_800625A0->details->digits_shown = 1;
}

/* Draw a nine-digit number (the party's gold) at (6bh, 54h), or lower at (53h, 64h). */
void func_801D0C20(u32 value, u8 lower) {
    s32 i;
    s32 x;
    s32 y;

    x = 0x68;
    y = 0x54;
    if (lower) {
        x = 0x50;
        y = 0x64;
    }
    func_801C5298(value);
    i = 0;
    D_800625A0->details->group1220_count = 0;
    for (; i < 9; i++, x += 8) {
        if (D_800625A0->digits[i] != 0xFF) {
            D_800625A0->details->group1220_count +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[i],
                              D_800625A0->details->group1220 + D_800625A0->details->group1220_count * 2,
                              D_800625A0->buffer, x + 3, y, 0x1000);
        }
    }
    D_800625A0->details->group1220_buffer = D_800625A0->buffer;
    D_800625A0->flags->unk5B = 2;
}

/* Draw a nine-digit price and its unit sprite at (aah, aeh). */
void func_801D0D4C(u32 value) {
    s32 i;
    s32 x;

    func_801C5298(value);
    i = 0;
    x = 0xAA;
    D_800625A0->details->price_count = 0;
    for (; i < 9; i++, x += 8) {
        if (D_800625A0->digits[i] != 0xFF) {
            D_800625A0->details->price_count +=
                func_8002675C(D_800625A0->sprite_sheet, D_800625A0->digits[i],
                              D_800625A0->details->price + D_800625A0->details->price_count * 2,
                              D_800625A0->buffer, x, 0xAE, 0x1000);
        }
    }
    D_800625A0->details->price_count +=
        func_8002675C(D_800625A0->sprite_sheet, 0x10,
                      D_800625A0->details->price + D_800625A0->details->price_count * 2, D_800625A0->buffer,
                      0xF2, 0xAE, 0x1000);
    D_800625A0->details->price_buffer = D_800625A0->buffer;
    D_800625A0->flags->price_shown = 1;
}

/* Hide the shop list's packets; with `close` also close its panels, scroll bar and marker. */
void func_801D0EC8(u8 close) {
    s32 i;

    D_800625A0->flags->unk5A = 0;
    D_800625A0->details->label4430_shown = 0;
    D_800625A0->details->label44B0_shown = 0;
    D_800625A0->details->digits_shown = 0;
    D_800625A0->details->heading_count = 0;
    D_800625A0->details->group2D0_count = 0;
    D_800625A0->details->members_count = 0;
    for (i = 0; i < 9; i++) {
        D_800625A0->details->bar_shown[i] = 0;
        D_800625A0->details->cells_a_count[i] = 0;
        D_800625A0->details->cells_b_count[i] = 0;
    }
    for (i = 0; i < 8; i++) {
        D_800625A0->details->name_shown[i] = 0;
        D_800625A0->details->row_count[i] = 0;
    }
    D_800625A0->details->label4530_shown = 0;
    if (close) {
        func_801C9054(2);
        func_801C9054(3);
        func_801C782C();
        func_801C7A88(0);
    }
}

/* Party bits of the available members whose gear has part `item` of kind `kind` fitted. */
u32 func_801D1078(u8 item, u8 kind) {
    u16 members;
    u8 found;
    s32 i;
    s32 k;

    members = 0;
    if (item != 0) {
        for (i = 0; i < 16; i++) {
            found = 0;
            if (D_800625A0->member_present[i] != 0) {
                switch (kind) {
                case 1:
                    if (D_8006DFAC[D_8006D8A0[i].unkA0].unk2 == item) {
                        found = 1;
                    }
                    break;
                case 0:
                    if (D_8006DFAC[D_8006D8A0[i].unkA0].unk8 == item) {
                        found = 1;
                    }
                    break;
                case 2:
                    if (D_8006DFAC[D_8006D8A0[i].unkA0].unk3 == item) {
                        found = 1;
                    }
                    break;
                case 4:
                    for (k = 0; k < 4; k++) {
                        if (item < 0x32) {
                            if (D_8006DFAC[D_8006D8A0[i].unkA0].unkC[k] == item) {
                                found = 1;
                                break;
                            }
                        } else if (D_8006DFAC[D_8006D8A0[i].unkA0].unk4[k] == item) {
                            found = 1;
                            break;
                        }
                    }
                    break;
                case 3:
                    for (k = 0; k < 3; k++) {
                        if (D_8006DFAC[D_8006D8A0[i].unkA0].unk9[k] == item) {
                            found = 1;
                            break;
                        }
                    }
                    break;
                }
            }
            if (found) {
                members |= func_801C5260(D_8006D8A0[i].unkA0);
            }
        }
    }
    return members;
}

/*
 * Show part `id` of kind `kind` (3 or 4): its name and sell price (half the
 * table price) labels, the bars of the members who can use it and the marks
 * of those holding it. Returns the sell price.
 */
/* Frame layout unresolved: the original reserves 16 additional bytes
 * that the recovered operations do not explain. */
#ifdef NON_MATCHING
u32 func_801D1304(u8 id, u8 kind) {
    RECT rect;
    u8 codes[14];
    u8 text[16];
    s32 divisors[5];
    u8 *pixels;
    u32 users;
    u32 price;
    s32 value;
    u32 holders;
    s32 digit;
    u8 started;
    u8 gear;
    s32 j;
    s32 i;

    users = 0;
    divisors[0] = 1;
    divisors[1] = 10;
    divisors[2] = 100;
    divisors[3] = 1000;
    divisors[4] = 10000;
    pixels = func_80031BDC(0x618, 0);
    bzero(pixels, 0x618);
    bzero(codes, 14);
    switch (kind) {
    case 4:
        D_800625A0->details->label4430.width = func_80034EAC(func_80033A5C(id), pixels, 0x39, 0);
        users = ((GearTable *)D_800625A0->resources)->weapons[id].users;
        price = ((GearTable *)D_800625A0->resources)->weapons[id].price >> 1;
        value = price;
        break;
    case 3:
        D_800625A0->details->label4430.width = func_80034EAC(func_80033A2C(id), pixels, 0x39, 0);
        users = ((GearTable *)D_800625A0->resources)->parts[id].users;
        price = ((GearTable *)D_800625A0->resources)->parts[id].price >> 1;
        value = price;
        break;
    }
    holders = func_801D1078(id, kind);
    started = 0;
    for (i = 0, j = 4; j > 0; i++, j--) {
        digit = value / divisors[j];
        if (digit != 0 || started) {
            codes[i * 2] = digit + 0x10;
            started = 1;
            value -= digit * divisors[j];
        } else {
            codes[i * 2] = 0xC3;
        }
    }
    codes[8] = value % 10 + 0x10;
    func_80033B34(codes, text, 5);
    D_800625A0->details->label44B0.width = func_80034EAC(text, pixels, 0x39, 1);
    rect.x = 0x140;
    rect.y = 0x4E;
    rect.w = 0x3C;
    rect.h = 13;
    LoadImage(&rect, pixels);
    DrawSync(0);
    func_801C5CA8(&D_800625A0->details->label4430, 0, 0, 0);
    func_801C5CA8(&D_800625A0->details->label44B0, 0, 0, 0);
    D_800625A0->details->label44B0.poly[D_800625A0->buffer].clut = D_80059414;
    func_801C51B8(&D_800625A0->details->label4430.poly[D_800625A0->buffer], 0x2C, 0x12, 0, 0x4E,
                  D_800625A0->details->label4430.width, 13);
    func_801C51B8(&D_800625A0->details->label44B0.poly[D_800625A0->buffer], 0x98, 0x12, 0, 0x4E,
                  D_800625A0->details->label44B0.width, 13);
    func_801C7604(D_800625A0->details->label4430.quad, 0x2C, 0x12, D_800625A0->details->label4430.width, 13);
    func_801C7604(D_800625A0->details->label44B0.quad, 0x98, 0x12, D_800625A0->details->label44B0.width, 13);
    D_800625A0->details->label4430.buffer = D_800625A0->buffer;
    D_800625A0->details->label44B0.buffer = D_800625A0->buffer;
    func_800320E8(pixels);
    if (id) {
        D_800625A0->details->label4430_shown = 1;
        D_800625A0->details->label44B0_shown = 1;
    } else {
        D_800625A0->details->label4430_shown = 0;
        D_800625A0->details->label44B0_shown = 0;
    }
    i = 0;
    j = 0;
    D_800625A0->details->group2D0_count = 0;
    for (; i < 16; i++) {
        if (D_800625A0->member_present[i] != 0) {
            gear = D_8006D8A0[i].unkA0;
            if (func_801C527C(users, gear)) {
                D_800625A0->details->bar_shown[j] = 1;
            } else {
                D_800625A0->details->bar_shown[j] = 0;
            }
            if (func_801C527C(holders, gear)) {
                D_800625A0->details->group2D0_count +=
                    func_8002675C(D_800625A0->sprite_sheet, 0xE,
                                  &D_800625A0->details->group2D0[D_800625A0->details->group2D0_count * 2],
                                  D_800625A0->buffer, D_801D6C44[j] + 0xE, 0xB4, 0x1000);
            }
            j++;
        }
    }
    D_800625A0->details->group2D0_buffer = D_800625A0->buffer;
    return price;
}
#else
INCLUDE_ASM(".local/decomp/ovl2602/asm/nonmatchings/gear_shop", func_801D1304);
#endif

/*
 * Draw the eight visible rows of a list from entry `top`: each part's name,
 * the count held and, when some are chosen, "x" and the chosen count.
 */
void func_801D18F8(s32 top, u8 *ids, u8 *kinds, u8 *chosen, u8 *held) {
    RECT rect;
    u8 codes[14];
    u8 text[16];
    s32 divisors[5];
    u8 *pixels;
    s32 value;
    s32 digit;
    u8 started;
    u8 tens;
    s32 row;
    s32 i;
    s32 j;

    divisors[0] = 1;
    divisors[1] = 10;
    divisors[2] = 100;
    divisors[3] = 1000;
    divisors[4] = 10000;
    pixels = func_80031BDC(0x3F6, 0);
    for (row = 0; row < 8; row++) {
        bzero(codes, 14);
        D_800625A0->details->row_count[row] = 0;
        if (ids[top + row] != 0) {
            value = held[top + row];
            switch (kinds[top + row]) {
            case 4:
                D_800625A0->details->names_a[row].width =
                    func_80034EAC(func_80033A5C(ids[top + row]), pixels, 0x24, 0);
                break;
            case 3:
                D_800625A0->details->names_a[row].width =
                    func_80034EAC(func_80033A2C(ids[top + row]), pixels, 0x24, 0);
                break;
            }
            started = 0;
            for (i = 0, j = 4; j > 0; i++, j--) {
                digit = value / divisors[j];
                if (digit != 0 || started) {
                    codes[i * 2] = digit + 0x10;
                    started = 1;
                    value -= digit * divisors[j];
                } else {
                    codes[i * 2] = 0xC3;
                }
            }
            codes[i * 2] = value % 10 + 0x10;
            func_80033B34(codes, text, 5);
            D_800625A0->details->names_b[row].width = func_80034EAC(text, pixels, 0x24, 1);
            rect.x = (row & 1) * 0x18 + 0x180;
            rect.y = (row / 2) * 13 + 0x80;
            rect.w = 0x28;
            rect.h = 13;
            LoadImage(&rect, pixels);
            func_801C5CA8(&D_800625A0->details->names_a[row], row, 0x80, 0x81);
            func_801C7604(D_800625A0->details->names_a[row].quad, 0x24, row * 13 + 0x32,
                          D_800625A0->details->names_a[row].width, 13);
            rect.x = (row & 1) * 0x18 + 0x180;
            rect.y = (row / 2) * 13 + 0x80;
            rect.w = 0x28;
            rect.h = 13;
            LoadImage(&rect, pixels);
            DrawSync(0);
            func_801C5CA8(&D_800625A0->details->names_b[row], row, 0x80, 0x82);
            func_801C7604(D_800625A0->details->names_b[row].quad, 0x8C, row * 13 + 0x32,
                          D_800625A0->details->names_b[row].width, 13);
            D_800625A0->details->names_a[row].buffer = D_800625A0->buffer;
            D_800625A0->details->names_a[row].buffer = D_800625A0->buffer;
            D_800625A0->details->name_shown[row] = 1;
            if (chosen[top + row] != 0) {
                D_800625A0->details->row_count[row] +=
                    func_8002675C(D_800625A0->sprite_sheet, 0xE5, D_800625A0->details->rows[row],
                                  D_800625A0->buffer, 0xB4, row * 13 + 0x36, 0x1000);
                tens = chosen[top + row] / 10;
                if (tens != 0) {
                    D_800625A0->details->row_count[row] +=
                        func_8002675C(D_800625A0->sprite_sheet, tens,
                                      &D_800625A0->details->rows[row][D_800625A0->details->row_count[row] * 2],
                                      D_800625A0->buffer, 0xBC, row * 13 + 0x36, 0x1000);
                }
                D_800625A0->details->row_count[row] +=
                    func_8002675C(D_800625A0->sprite_sheet, (u8)(chosen[top + row] % 10),
                                  &D_800625A0->details->rows[row][D_800625A0->details->row_count[row] * 2],
                                  D_800625A0->buffer, 0xC4, row * 13 + 0x36, 0x1000);
                D_800625A0->details->row_buffer[row] = D_800625A0->buffer;
            }
        } else {
            D_800625A0->details->name_shown[row] = 0;
        }
    }
    func_800320E8(pixels);
}

/* Set the party's gold (capped at 9999999) and, with `remove`, take the chosen amounts out of the inventory. */
void func_801D1F20(u32 gold, u8 *ids, u8 *amounts, s32 n, u8 *inv_ids, u8 *inv_counts, u8 *unused,
                   u8 remove, u8 unused_member) {
    u32 *party_gold;
    s32 i;
    s32 j;

    func_801CB498(0xD1);
    party_gold = &D_8006EF58;
    *party_gold = gold;
    if (gold > 9999999) {
        *party_gold = 9999999;
    }
    if (remove) {
        for (i = 0; i < n; i++) {
            if (ids[i] != 0) {
                for (j = 0; j < n; j++) {
                    if (ids[i] == inv_ids[j]) {
                        inv_counts[j] -= amounts[i];
                        if (inv_counts[j] == 0) {
                            inv_ids[j] = 0;
                        }
                    }
                }
            }
        }
    }
}

/*
 * The sell list (twin of the item shop's): choose how many of each of `n`
 * held parts to sell, eight rows at a time, with the running total and the
 * gold after the sale; confirming settles the sale. Every row uses `kind`;
 * `remove` controls inventory removal and the settlement ignores `member`.
 */
void func_801D2054(s32 n, u8 *ids, u8 *counts, u8 kind, u8 remove, u8 *unused_kinds, u8 member) {
    u8 running = 1;
    u8 first = 1;
    u8 redraw = 1;
    s32 row = 0;
    s32 last_row = 0xFF;
    s32 top = 0;
    s32 last_top = 0xFF;
    u8 sell_ids[n];
    u8 sell_kinds[n];
    u8 chosen[n];
    u8 selectable[n];
    u8 *held;
    u32 gold = D_8006EF58;
    u8 held_storage[n];
    u32 total;
    u32 new_gold;
    u32 price;
    s32 count;
    s32 collected;
    s32 i;
    s32 index;

    held = held_storage;
    new_gold = gold;
    total = 0;

    for (i = 0; i < n; i++) {
        held[i] = 0;
        selectable[i] = 0;
        chosen[i] = 0;
        sell_ids[i] = 0;
        sell_kinds[i] = kind;
    }
    collected = 0;
    for (i = 0; i < n; i++) {
        if (ids[i] != 0 && counts[i] != 0) {
            sell_ids[collected] = ids[i];
            held[collected] = counts[i];
            selectable[collected] = 1;
            collected++;
        }
    }
    count = collected;
    func_801C7870(0);
    while (running) {
        func_801CC1C4();
        if (top != last_top || redraw) {
            func_801D18F8(top, sell_ids, sell_kinds, chosen, held);
            func_801C76A4(0xC, 0x32, 0x3C, count, top);
        }
        if (row != last_row || top != last_top) {
            last_row = row;
            price = func_801D1304(sell_ids[top + row], sell_kinds[top + row]);
            last_top = top;
            D_800625A0->flags->unk5A = 1;
        }
        func_801C78EC(row, top, 0, 0);
        if (first) {
            func_801C90E0(2, 0xC, 0x2A, 0xC4, 0x74, 0, 1, 4, 1);
            func_801C90E0(3, 0x20, 0xE, 0xFC, 0x14, 0, 1, 4, 0);
            func_801D0220();
            func_801D05EC();
            first = 0;
        }
        if (redraw) {
            func_801D06D8(gold, total, new_gold, 0, 0);
            redraw = 0;
        }
        switch (D_800625A0->input) {
        case 4:
            if (total != 0) {
                func_801CB498(2);
                D_800625A0->flags->unk5A = 0;
                D_800625A0->flags->panel_shown[2] = 0;
                D_800625A0->flags->panel_shown[3] = 0;
                D_800625A0->flags->scroll_shown = 0;
                running = 0;
                D_800625A0->flags->marker_shown[0] = 0;
                func_801CC31C(0);
                func_801D0C20(total, 0);
                if (func_801CCC18(0x95, 0xFF, 1)) {
                    func_801D1F20(new_gold, sell_ids, chosen, n, ids, counts, sell_kinds, remove, member);
                } else {
                    running = 1;
                    D_800625A0->flags->panel_shown[2] = 1;
                    D_800625A0->flags->panel_shown[3] = 1;
                    D_800625A0->flags->scroll_shown = 1;
                    last_top = 0xFF;
                    last_row = 0xFF;
                    D_800625A0->flags->marker_shown[0] = running;
                }
                func_801CC4DC();
            } else {
                func_801CB498(4);
            }
            break;
        case 5:
            running = 0;
            if (total != 0) {
                D_800625A0->flags->unk5A = 0;
                D_800625A0->flags->panel_shown[2] = 0;
                D_800625A0->flags->panel_shown[3] = 0;
                D_800625A0->flags->scroll_shown = 0;
                D_800625A0->flags->marker_shown[0] = 0;
                func_801CC31C(0);
                if (!func_801CCC18(0x92, 0xFF, 1)) {
                    running = 1;
                    D_800625A0->flags->panel_shown[2] = 1;
                    D_800625A0->flags->panel_shown[3] = 1;
                    D_800625A0->flags->scroll_shown = 1;
                    last_top = 0xFF;
                    last_row = 0xFF;
                    D_800625A0->flags->marker_shown[0] = running;
                }
                func_801CC4DC();
            }
            break;
        case 1:
            row++;
            if (row >= 8) {
                row = 7;
                if (count - 8 < ++top) {
                    top--;
                }
            }
            break;
        case 3:
            row--;
            if (row < 0) {
                top--;
                row = 0;
                if (top < 0) {
                    top = 0;
                }
            }
            break;
        case 0:
            index = top + row;
            if (selectable[index] && held[index] - 1 >= 0) {
                redraw = 1;
                total += price;
                chosen[index]++;
                new_gold += price;
                held[index]--;
            }
            break;
        case 2:
            index = top + row;
            if (selectable[index] && chosen[index] - 1 >= 0) {
                total -= price;
                redraw = 1;
                held[index]++;
                new_gold -= price;
                chosen[index]--;
            }
            break;
        }
    }
}

/* Run the sell list for inventory 3 (150 entries). */
void func_801D2784(void) {
    func_801D2054(150, D_8006F84E, D_8006F84E - 150, 3, 1, D_8006F84E - 150, 0);
}

/* Run the sell list for inventory 4 (100 entries). */
void func_801D27C4(void) {
    func_801D2054(100, D_8006F754, D_8006F754 - 100, 4, 1, D_8006F754 - 100, 0);
}

/* Run sell list `page` * 3 + cursor (3 and 4 are the two inventories), then restore the list labels. */
void func_801D2804(u8 page) {
    u8 close;

    D_800625A0->flags->unk4 = 0;
    D_800625A0->flags->cursor_shown = 0;
    D_800625A0->flags->lists_shown = 0;
    func_801CCEBC(4, D_800625A0->flags->list_label_shown);
    close = 1;
    switch (D_800625A0->list_cursor + page * 3) {
    case 3:
        func_801D2784();
        break;
    case 4:
        func_801D27C4();
        break;
    }
    func_801D0EC8(close);
    D_800625A0->flags->lists_shown = 1;
    D_800625A0->flags->unk4 = 1;
    D_800625A0->flags->cursor_shown = 1;
    func_801CCE90(4, D_800625A0->list_labels, D_801D6A24, D_800625A0->flags->list_label_shown);
}

/* Render the edited gear's part name of list `kind` (0 frame, 1 engine, 2 armour) into its label and show it. */
void func_801D2950(u8 kind) {
    RECT rect;
    u8 *pixels;

    pixels = func_80031BDC(0x3F6, 0);
    switch (kind) {
    case 0:
        D_800625A0->details->label4530.width = func_80034EAC(
            func_80033728(D_800625A0->details->resources[6], D_8006D634.gears[D_801D9084].unk8), pixels, 0x24, 0);
        break;
    case 1:
        D_800625A0->details->label4530.width = func_80034EAC(
            func_80033728(D_800625A0->details->resources[7], D_8006D634.gears[D_801D9084].unk2), pixels, 0x24, 0);
        break;
    case 2:
        D_800625A0->details->label4530.width = func_80034EAC(
            func_80033728(D_800625A0->details->resources[8], D_8006D634.gears[D_801D9084].unk3), pixels, 0x24, 0);
        break;
    }
    rect.x = 0x198;
    rect.y = 0xB4;
    rect.w = 0x28;
    rect.h = 0xD;
    LoadImage(&rect, pixels);
    func_801C5CA8(&D_800625A0->details->label4530, 9, 0x80, 0x81);
    func_801C7604(D_800625A0->details->label4530.quad, 0xD4, 0x8E, D_800625A0->details->label4530.width, 0xD);
    DrawSync(0);
    D_800625A0->details->label4530.buffer = D_800625A0->buffer;
    D_800625A0->details->label4530_shown = 1;
    func_800320E8(pixels);
}

/*
 * Draw the eight visible rows of the parts shop's stock from entry `top`:
 * each part's name and price (`dims[row]` 80h when `gold` covers it and, for
 * kinds 0-2, the edited gear can fit it) and, when some are chosen, "x" and
 * the amount. Returns half the price of the edited gear's fitted part of the
 * last row's kind (0-2): its trade-in value.
 */
u32 func_801D2B74(s32 top, s32 gold, u8 *dims) {
    RECT rect;
    u8 codes[14];
    u8 text[16];
    s32 divisors[5];
    u32 users;
    u32 trade;
    s32 price;
    GearEntry *frame;
    GearRecord18 *engine;
    GearRecord10 *armour;
    s32 value;
    u8 *pixels;
    s32 digit;
    u8 started;
    u8 tens;
    s32 row;
    s32 i;
    s32 j;

    divisors[0] = 1;
    divisors[1] = 10;
    divisors[2] = 100;
    divisors[3] = 1000;
    divisors[4] = 10000;
    pixels = func_80031BDC(0x3F6, 0);
    for (row = 0; row < 8; row++) {
        dims[row] = 0;
        bzero(codes, 14);
        D_800625A0->details->row_count[row] = 0;
        if (D_800625A0->shop_items[top + row] != 0) {
            switch (D_800625A0->shop_kinds[top + row]) {
            case 0:
                frame = &((GearTable *)D_800625A0->resources)->entries[D_8006DFAC[D_801D9084].unk8];
                trade = frame->price >> 1;
                D_800625A0->details->names_a[row].width = func_80034EAC(
                    func_80033728(D_800625A0->details->resources[6], D_800625A0->shop_items[top + row]), pixels, 0x24, 0);
                frame = &((GearTable *)D_800625A0->resources)->entries[D_800625A0->shop_items[top + row]];
                value = frame->price;
                price = value;
                users = frame->users;
                break;
            case 1:
                engine = &((GearTable *)D_800625A0->resources)->records18[D_8006DFAC[D_801D9084].unk2];
                trade = engine->price >> 1;
                D_800625A0->details->names_a[row].width = func_80034EAC(
                    func_80033728(D_800625A0->details->resources[7], D_800625A0->shop_items[top + row]), pixels, 0x24, 0);
                engine = &((GearTable *)D_800625A0->resources)->records18[D_800625A0->shop_items[top + row]];
                value = engine->price;
                price = value;
                users = engine->users;
                break;
            case 2:
                armour = &((GearTable *)D_800625A0->resources)->records10[D_8006DFAC[D_801D9084].unk3];
                trade = armour->price >> 1;
                D_800625A0->details->names_a[row].width = func_80034EAC(
                    func_80033728(D_800625A0->details->resources[8], D_800625A0->shop_items[top + row]), pixels, 0x24, 0);
                armour = &((GearTable *)D_800625A0->resources)->records10[D_800625A0->shop_items[top + row]];
                value = armour->price;
                price = value;
                users = armour->users;
                break;
            case 4:
                D_800625A0->details->names_a[row].width =
                    func_80034EAC(func_80033A5C(D_800625A0->shop_items[top + row]), pixels, 0x24, 0);
                value = ((GearTable *)D_800625A0->resources)->weapons[D_800625A0->shop_items[top + row]].price;
                price = value;
                users = -1;
                break;
            case 3:
                D_800625A0->details->names_a[row].width =
                    func_80034EAC(func_80033A2C(D_800625A0->shop_items[top + row]), pixels, 0x24, 0);
                value = ((GearTable *)D_800625A0->resources)->parts[D_800625A0->shop_items[top + row]].price;
                price = value;
                users = -1;
                break;
            }
            if (gold >= price) {
                if (func_801C527C(users, D_801D9084)) {
                    dims[row] = 0x80;
                }
            }
            started = 0;
            for (i = 0, j = 4; j > 0; i++, j--) {
                digit = value / divisors[j];
                if (digit != 0 || started) {
                    codes[i * 2] = digit + 0x10;
                    started = 1;
                    value -= digit * divisors[j];
                } else {
                    codes[i * 2] = 0xC3;
                }
            }
            codes[i * 2] = value % 10 + 0x10;
            func_80033B34(codes, text, 5);
            D_800625A0->details->names_b[row].width = func_80034EAC(text, pixels, 0x24, 1);
            rect.x = (row & 1) * 0x18 + 0x180;
            rect.y = (row / 2) * 13 + 0x80;
            rect.w = 0x28;
            rect.h = 13;
            LoadImage(&rect, pixels);
            func_801C5CA8(&D_800625A0->details->names_a[row], row, 0x80, dims[row] + 1);
            func_801C7604(D_800625A0->details->names_a[row].quad, 0x24, row * 13 + 0x32,
                          D_800625A0->details->names_a[row].width, 13);
            rect.x = (row & 1) * 0x18 + 0x180;
            rect.y = (row / 2) * 13 + 0x80;
            rect.w = 0x28;
            rect.h = 13;
            LoadImage(&rect, pixels);
            DrawSync(0);
            func_801C5CA8(&D_800625A0->details->names_b[row], row, 0x80, dims[row] + 2);
            func_801C7604(D_800625A0->details->names_b[row].quad, 0x8C, row * 13 + 0x32,
                          D_800625A0->details->names_b[row].width, 13);
            D_800625A0->details->names_a[row].buffer = D_800625A0->buffer;
            D_800625A0->details->names_a[row].buffer = D_800625A0->buffer;
            D_800625A0->details->name_shown[row] = 1;
            if (D_800625A0->details->amounts[top + row] != 0) {
                D_800625A0->details->row_count[row] +=
                    func_8002675C(D_800625A0->sprite_sheet, 0xF1, D_800625A0->details->rows[row],
                                  D_800625A0->buffer, 0xB4, row * 13 + 0x36, 0x1000);
                tens = D_800625A0->details->amounts[top + row] / 10;
                if (tens != 0) {
                    D_800625A0->details->row_count[row] +=
                        func_8002675C(D_800625A0->sprite_sheet, tens,
                                      &D_800625A0->details->rows[row][D_800625A0->details->row_count[row] * 2],
                                      D_800625A0->buffer, 0xBC, row * 13 + 0x36, 0x1000);
                }
                D_800625A0->details->row_count[row] +=
                    func_8002675C(D_800625A0->sprite_sheet, (u8)(D_800625A0->details->amounts[top + row] % 10),
                                  &D_800625A0->details->rows[row][D_800625A0->details->row_count[row] * 2],
                                  D_800625A0->buffer, 0xC4, row * 13 + 0x36, 0x1000);
                D_800625A0->details->row_buffer[row] = D_800625A0->buffer;
            }
        } else {
            D_800625A0->details->name_shown[row] = 0;
        }
    }
    func_800320E8(pixels);
    return trade;
}

/*
 * Preview fitting part `part` of list `kind` (3 parts, 4 weapons) to member
 * `member`'s gear: fit it (a part of the same type, else in place of the
 * lowest-ranked part; a weapon of the same class), recompute the gear, and
 * return the change of its two shown values against the stored ones as
 * magnitudes and signs (1 a decrease); then restore the gear.
 */
void func_801D3558(s32 *change, u8 *decrease, u8 part, u8 kind, u8 member) {
    s16 values[2][4]; /* preview and saved stats, with an eight-byte row stride */
    u8 saved[8];
    GearWeapon *weapon;
    GearPart *fitted;
    GearPart *current;
    u8 gear;
    u8 replace;
    u8 lowest;
    s32 slot;
    s32 k;

    gear = D_8006D8A0[member].unkA0;
    saved[0] = D_8006DFAC[gear].unkC[0];
    saved[1] = D_8006DFAC[gear].unk4[0];
    saved[2] = D_8006DFAC[gear].unk4[1];
    saved[3] = D_8006DFAC[gear].unk4[2];
    saved[4] = D_8006DFAC[gear].unk4[3];
    saved[5] = D_8006DFAC[gear].unk9[0];
    saved[6] = D_8006DFAC[gear].unk9[1];
    saved[7] = D_8006DFAC[gear].unk9[2];
    switch (kind) {
    case 4:
        if (part < 0x32) {
            D_8006DFAC[gear].unkC[0] = part;
        } else {
            weapon = &((GearTable *)D_800625A0->resources)->weapons[part];
            for (k = 0; k < 4; k++) {
                if (((GearTable *)D_800625A0->resources)->weapons[D_8006DFAC[gear].unk4[k]].unkF == weapon->unkF) {
                    D_8006DFAC[gear].unk4[k] = part;
                }
            }
        }
        break;
    case 3:
        replace = 1;
        fitted = &((GearTable *)D_800625A0->resources)->parts[part];
        for (k = 0; k < 3; k++) {
            current = &((GearTable *)D_800625A0->resources)->parts[saved[5 + k]];
            if (current->unk8 != 0 && current->unk8 == fitted->unk8) {
                replace = 0;
                D_8006DFAC[gear].unk9[k] = part;
            }
        }
        if (replace) {
            lowest = 0xFF;
            for (k = 0; k < 3; k++) {
                current = &((GearTable *)D_800625A0->resources)->parts[saved[5 + k]];
                if (lowest >= current->unkD) {
                    lowest = current->unkD;
                    slot = k;
                }
            }
            D_8006DFAC[gear].unk9[slot] = part;
        }
        break;
    }
    func_801D6150((GearTable *)D_800625A0->resources, D_8006D8A0[member].unkA0);
    func_801D5F94((GearTable *)D_800625A0->resources, D_8006D8A0[member].unkA0);
    values[0][0] = ((GearTable *)D_800625A0->resources)->unkB0;
    values[0][1] = ((GearTable *)D_800625A0->resources)->unkA4;
    values[1][0] = D_800625A0->details->stat_b0[member];
    values[1][1] = D_800625A0->details->stat_a4[member];
    for (k = 0; k < 2; k++) {
        if (values[0][k] >= values[1][k]) {
            change[k] = values[0][k] - values[1][k];
            decrease[k] = 0;
        } else {
            change[k] = values[1][k] - values[0][k];
            decrease[k] = 1;
        }
    }
    D_8006DFAC[gear].unkC[0] = saved[0];
    D_8006DFAC[gear].unk4[0] = saved[1];
    D_8006DFAC[gear].unk4[1] = saved[2];
    D_8006DFAC[gear].unk4[2] = saved[3];
    D_8006DFAC[gear].unk4[3] = saved[4];
    D_8006DFAC[gear].unk9[0] = saved[5];
    D_8006DFAC[gear].unk9[1] = saved[6];
    D_8006DFAC[gear].unk9[2] = saved[7];
    func_801D6150((GearTable *)D_800625A0->resources, D_8006D8A0[member].unkA0);
}

/* The count held of item `id` in an inventory of `n` ids and counts (0 if absent). */
u16 func_801D3A3C(u8 *ids, u8 *counts, s32 n, u8 id) {
    u8 count;
    s32 i;

    count = 0;
    for (i = 0; i < n; i++) {
        if (ids[i] == id) {
            count = counts[i];
            break;
        }
    }
    return count;
}

/* Show how many of item `id` the party holds in inventory `kind` (3 or 4) beside the list. */
void func_801D3A80(u8 kind, u8 id) {
    RECT rect;
    u8 codes[4];
    u8 text[8];
    u8 *ids;
    u8 *counts;
    s32 n;
    u8 *pixels;
    u16 count;
    u8 known;

    known = 0;
    switch (kind) {
    case 4:
        ids = D_8006F754;
        counts = ids - 100;
        n = 100;
        known = 1;
        break;
    case 3:
        ids = D_8006F84E;
        counts = ids - 150;
        n = 150;
        known = 1;
        break;
    }
    if (!known) {
        return;
    }
    count = func_801D3A3C(ids, counts, n, id);
    D_801D904C = count;
    pixels = func_80031BDC(0x3F6, 0);
    codes[1] = 0;
    codes[3] = 0;
    if (count / 10) {
        codes[0] = count / 10 + 0x10;
    } else {
        codes[0] = 0xC3;
    }
    codes[2] = count % 10 + 0x10;
    func_80033B34(codes, text, 2);
    D_800625A0->details->label45B0.width = func_80034EAC(text, pixels, 0x24, 1);
    rect.x = 0x198;
    rect.y = 0xB4;
    rect.w = 0x28;
    rect.h = 13;
    LoadImage(&rect, pixels);
    DrawSync(0);
    func_801C5CA8(&D_800625A0->details->label45B0, 9, 0x80, 0x82);
    func_801C7604(D_800625A0->details->label45B0.quad, 0xF8, 0x8E, D_800625A0->details->label45B0.width,
                  13);
    D_800625A0->details->label45B0.buffer = D_800625A0->buffer;
    D_800625A0->details->label45B0_shown = 1;
    func_800320E8(pixels);
}

/*
 * Show parts-shop entry `top + row` (`dims` unused): its name label, the bars
 * of the members whose gear can fit it, the marks of those whose gear holds it
 * and, for parts (kinds 3 and 4) each fitting member's attack and defence
 * change (tinted by whether it drops), then how many the party holds.
 * Returns its price.
 */
u32 func_801D3C78(s32 row, s32 top, u8 *dims) {
    RECT rect;
    s32 diffs[2];
    u8 worse[2];
    u32 price;
    u32 holders;
    u32 users;
    u8 id;
    u8 kind;
    u8 *pixels;
    u8 gear;
    s32 i;
    s32 shown;
    s32 k;
    s32 xa;
    s32 xb;

    id = D_800625A0->shop_items[top + row];
    kind = D_800625A0->shop_kinds[top + row];
    pixels = func_80031BDC(0x618, 0);
    bzero(pixels, 0x618);
    users = 0;
    switch (kind) {
    case 0:
        D_800625A0->details->label4430.width =
            func_80034EAC(func_80033728(D_800625A0->details->resources[3], id), pixels, 0x39, 0);
        price = ((GearTable *)D_800625A0->resources)->entries[id].price;
        users = ((GearTable *)D_800625A0->resources)->entries[id].users;
        break;
    case 1:
        D_800625A0->details->label4430.width =
            func_80034EAC(func_80033728(D_800625A0->details->resources[4], id), pixels, 0x39, 0);
        price = ((GearTable *)D_800625A0->resources)->records18[id].price;
        users = ((GearTable *)D_800625A0->resources)->records18[id].users;
        break;
    case 2:
        D_800625A0->details->label4430.width =
            func_80034EAC(func_80033728(D_800625A0->details->resources[5], id), pixels, 0x39, 0);
        price = ((GearTable *)D_800625A0->resources)->records10[id].price;
        users = ((GearTable *)D_800625A0->resources)->records10[id].users;
        break;
    case 4:
        D_800625A0->details->label4430.width =
            func_80034EAC(func_80033728(D_800625A0->details->resources[1], id), pixels, 0x39, 0);
        price = ((GearTable *)D_800625A0->resources)->weapons[id].price;
        users = ((GearTable *)D_800625A0->resources)->weapons[id].users;
        break;
    case 3:
        D_800625A0->details->label4430.width =
            func_80034EAC(func_80033728(D_800625A0->details->resources[2], id), pixels, 0x39, 0);
        price = ((GearTable *)D_800625A0->resources)->parts[id].price;
        users = ((GearTable *)D_800625A0->resources)->parts[id].users;
        break;
    }
    holders = func_801D1078(id, kind);
    rect.x = 0x140;
    rect.y = 0x4E;
    rect.w = 0x3C;
    rect.h = 13;
    LoadImage(&rect, pixels);
    DrawSync(0);
    func_801C5CA8(&D_800625A0->details->label4430, 0, 0, 0);
    func_801C51B8(&D_800625A0->details->label4430.poly[D_800625A0->buffer], 0x2C, 0x12, 0, 0x4E,
                  D_800625A0->details->label4430.width, 13);
    func_801C7604(D_800625A0->details->label4430.quad, 0x2C, 0x12, D_800625A0->details->label4430.width, 13);
    D_800625A0->details->label4430.buffer = D_800625A0->buffer;
    func_800320E8(pixels);
    if (id) {
        D_800625A0->details->label4430_shown = 1;
    } else {
        D_800625A0->details->label4430_shown = 0;
    }
    i = 0;
    shown = 0;
    D_800625A0->details->group2D0_count = 0;
    for (; i < 16; i++) {
        if (D_800625A0->member_present[i] != 0) {
            gear = D_8006D8A0[i].unkA0;
            if (func_801C527C(users, gear)) {
                D_800625A0->details->bar_shown[shown] = 1;
            } else {
                D_800625A0->details->bar_shown[shown] = 0;
            }
            if (func_801C527C(holders, gear)) {
                D_800625A0->details->group2D0_count +=
                    func_8002675C(D_800625A0->sprite_sheet, 0xE,
                                  &D_800625A0->details->group2D0[D_800625A0->details->group2D0_count * 2],
                                  D_800625A0->buffer, D_801D6C44[shown] + 0xE, 0xB4, 0x1000);
            }
            D_800625A0->details->cells_a_count[shown] = 0;
            D_800625A0->details->cells_b_count[shown] = 0;
            if ((kind == 3 || kind == 4) && D_800625A0->details->bar_shown[shown] != 0) {
                diffs[1] = 0;
                diffs[0] = 0;
                func_801D3558(diffs, worse, id, kind, i);
                if (diffs[0] != 0) {
                    func_801C5298(diffs[0]);
                    for (k = 0, xa = shown * 26 + 0x49; k < 3; k++) {
                        if (D_800625A0->digits[k + 6] != 0xFF) {
                            D_800625A0->details->cells_a_count[shown] += func_8002675C(
                                D_800625A0->sprite_sheet, D_800625A0->digits[k + 6],
                                &D_800625A0->details->cells_a[shown][D_800625A0->details->cells_a_count[shown] * 2],
                                D_800625A0->buffer, xa + k * 8, 0xBE, 0x1000);
                        }
                    }
                    func_801D0054(D_800625A0->details->cells_a_count[shown], D_800625A0->details->cells_a[shown],
                                  worse[0]);
                    D_800625A0->details->cells_a_buffer[shown] = D_800625A0->buffer;
                }
                if (diffs[1] != 0) {
                    func_801C5298(diffs[1]);
                    for (k = 0, xb = shown * 26 + 0x49; k < 3; k++) {
                        if (D_800625A0->digits[k + 6] != 0xFF) {
                            D_800625A0->details->cells_b_count[shown] += func_8002675C(
                                D_800625A0->sprite_sheet, D_800625A0->digits[k + 6],
                                &D_800625A0->details->cells_b[shown][D_800625A0->details->cells_b_count[shown] * 2],
                                D_800625A0->buffer, xb + k * 8, 0xC6, 0x1000);
                        }
                    }
                    func_801D0054(D_800625A0->details->cells_b_count[shown], D_800625A0->details->cells_b[shown],
                                  worse[1]);
                    D_800625A0->details->cells_b_buffer[shown] = D_800625A0->buffer;
                }
            }
            shown++;
        }
    }
    D_800625A0->details->group2D0_buffer = D_800625A0->buffer;
    func_801D3A80(kind, id);
    return price;
}

/*
 * Set the party's gold (capped at 9999999) and apply the purchases: kinds
 * 0-2 are fitted to the gear being edited, kinds 3 and 4 go into their
 * inventories (added to a part already held, at most 99, or into the first
 * free slot).
 */
void func_801D44FC(u32 gold) {
    u32 *party_gold;
    s32 i;
    s32 j;
    u8 new_item;

    func_801CB498(0xD1);
    party_gold = &D_8006EF58;
    *party_gold = gold;
    if (gold > 9999999) {
        *party_gold = 9999999;
    }
    for (i = 0; i < 0x30; i++) {
        if (D_800625A0->shop_items[i] != 0 && D_800625A0->details->amounts[i] != 0) {
            switch (D_800625A0->shop_kinds[i]) {
            case 0:
                D_8006DFAC[D_801D9084].unk8 = D_800625A0->shop_items[i];
                break;
            case 1:
                D_8006DFAC[D_801D9084].unk2 = D_800625A0->shop_items[i];
                break;
            case 2:
                D_8006DFAC[D_801D9084].unk3 = D_800625A0->shop_items[i];
                break;
            case 4:
                new_item = 1;
                for (j = 0; j < 100; j++) {
                    if (D_8006F754[j] == D_800625A0->shop_items[i]) {
                        new_item = 0;
                        if ((D_8006F6F0[j] += D_800625A0->details->amounts[i]) >= 100) {
                            D_8006F6F0[j] = 99;
                        }
                    }
                }
                if (new_item) {
                    for (j = 0; j < 100; j++) {
                        if (D_8006F754[j] == 0) {
                            D_8006F754[j] = D_800625A0->shop_items[i];
                            D_8006F6F0[j] = D_800625A0->details->amounts[i];
                            break;
                        }
                    }
                }
                break;
            case 3:
                new_item = 1;
                for (j = 0; j < 150; j++) {
                    if (D_8006F84E[j] == D_800625A0->shop_items[i]) {
                        new_item = 0;
                        if ((D_8006F7B8[j] += D_800625A0->details->amounts[i]) >= 100) {
                            D_8006F7B8[j] = 99;
                        }
                    }
                }
                if (new_item) {
                    for (j = 0; j < 150; j++) {
                        if (D_8006F84E[j] == 0) {
                            D_8006F84E[j] = D_800625A0->shop_items[i];
                            D_8006F7B8[j] = D_800625A0->details->amounts[i];
                            break;
                        }
                    }
                }
                break;
            }
        }
    }
}

/* Whether shop part `index` differs from the edited gear's part of that kind (0 when the gear already has it or better). */
u8 func_801D4888(s32 index) {
    u8 wanted;

    wanted = 1;
    switch (D_800625A0->shop_kinds[index]) {
    case 0:
        if (D_8006DFAC[D_801D9084].unk8 >= D_800625A0->shop_items[index]) {
            wanted = 0;
        }
        break;
    case 1:
        if (D_8006DFAC[D_801D9084].unk2 >= D_800625A0->shop_items[index]) {
            wanted = 0;
        }
        break;
    case 2:
        if (D_8006DFAC[D_801D9084].unk3 >= D_800625A0->shop_items[index]) {
            wanted = 0;
        }
        break;
    }
    return wanted;
}

/*
 * The parts shop's buy list `page` * 3 + cursor. With `fit` (lists 0-2) one
 * part is fitted to the edited gear, trading in its current part; otherwise
 * (lists 3 and 4) parts are bought by amount into the inventories.
 */
u8 func_801D498C(u8 page, u8 fit) {
    u8 dims[8];
    u8 running;
    u8 redraw;
    s32 row;
    u8 first;
    s32 top;
    s32 last_row;
    u32 total;
    s32 last_top;
    u32 gold;
    u32 new_gold;
    u32 funds;
    u32 trade_in;
    u32 credit;
    u8 message;
    u8 confirm;
    s32 panel_x;
    s32 panel_w;
    s32 count;
    u32 price;
    s32 i;
    s32 held_next;
    u8 *stock;

    gold = D_8006D634.gold;
    running = 1;
    first = 1;
    redraw = 1;
    row = 0;
    last_row = 0xFF;
    top = 0;
    last_top = 0xFF;
    total = 0;
    message = 0x8F;
    confirm = 0xFF;
    panel_x = 0xE0;
    credit = 0;
    panel_w = 0x40;
    new_gold = gold;
    funds = new_gold;
    for (i = 0; i < 11; i++) {
        if (D_8006D634.characters[i].unkA0 != 0xFF) {
            func_801D6150((GearTable *)D_800625A0->resources, D_8006D634.characters[i].unkA0);
            func_801D5F94((GearTable *)D_800625A0->resources, D_8006D634.characters[i].unkA0);
            D_800625A0->details->stat_b0[i] = ((GearTable *)D_800625A0->resources)->unkB0;
            D_800625A0->details->stat_a4[i] = ((GearTable *)D_800625A0->resources)->unkA4;
        }
    }
    bzero(D_800625A0->shop_items, 0x30);
    bzero(D_800625A0->shop_kinds, 0x30);
    bzero(D_800625A0->details->amounts, 0x30);
    for (i = 0; i < 20; i++) {
        stock = (u8 *)D_800625A0->details->stock;
        stock += (page * 3 + D_800625A0->list_cursor) * 20 + i;
        D_800625A0->shop_items[i] = *stock;
        D_800625A0->shop_kinds[i] = page * 3 + D_800625A0->list_cursor;
    }
    count = D_801D908C[page * 3 + D_800625A0->list_cursor];
    D_800625A0->images->dim = 1;
    func_801C7870(0);
    while (running) {
        func_801CC1C4();
        if (top != last_top || redraw) {
            if (fit) {
                trade_in = func_801D2B74(top, funds, dims);
            } else {
                trade_in = func_801D2B74(top, new_gold, dims);
            }
            func_801C76A4(0xC, 0x32, 0x3C, count, top);
        }
        if (row != last_row || top != last_top) {
            price = func_801D3C78(row, top, dims);
            last_row = row;
            last_top = top;
            D_800625A0->flags->unk5A = 1;
            if (fit) {
                panel_x = 0xC8;
                panel_w = 0x70;
                redraw = 1;
                new_gold = funds;
                total = 0;
                credit = 0;
                if (dims[row] != 0) {
                    total = price;
                    new_gold -= price;
                    credit = trade_in;
                    new_gold += credit;
                }
            }
        }
        func_801C78EC(row, top, 0, 0);
        if (first) {
            func_801C90E0(2, 0xC, 0x2A, 0xC4 - fit * 0x14, 0x74, 0, 1, 4, 1);
            func_801C90E0(3, 0x20, 0xE, 0xFC, 0x14, 0, 1, 4, 0);
            func_801C90E0(5, panel_x, 0x7A, panel_w, 0x24, 0, 1, 4, 0);
            func_801CCE90(2, D_800625A0->list_labels, D_801D6A2C, D_800625A0->flags->list_label_shown);
            func_801CCEE8(2, D_800625A0->list_labels, D_801D6A2C, D_801D6A40, D_800625A0->flags->list_label_shown,
                          fit, 0, 1);
            if (fit) {
                func_801D2950(D_800625A0->list_cursor + page * 3);
            }
            func_801D0220();
            func_801D04E8(fit);
            first = 0;
        }
        if (redraw) {
            func_801D06D8(gold, total, new_gold, credit, fit);
            redraw = 0;
        }
        switch (D_800625A0->input) {
        case 4:
            if (total != 0) {
                func_801CB498(2);
                D_800625A0->flags->unk5A = 0;
                D_800625A0->flags->panel_shown[2] = 0;
                D_800625A0->flags->panel_shown[3] = 0;
                D_800625A0->flags->scroll_shown = 0;
                D_800625A0->flags->marker_shown[0] = 0;
                D_800625A0->flags->panel_shown[5] = 0;
                running = 0;
                D_800625A0->flags->list_label_shown[fit] = 0;
                if (fit) {
                    if (func_801D4888(top + row)) {
                        message = 0xA3;
                    } else {
                        message = 0xAF;
                        confirm = 0xB2;
                    }
                } else {
                    func_801D0C20(total, 0);
                }
                func_801CC31C(0);
                if (func_801CCC18(message, confirm, 1)) {
                    if (fit) {
                        D_800625A0->details->amounts[top + row] = 1;
                    }
                    func_801D44FC(new_gold);
                } else {
                    running = 1;
                    D_800625A0->flags->panel_shown[2] = 1;
                    D_800625A0->flags->panel_shown[3] = 1;
                    D_800625A0->flags->panel_shown[5] = 1;
                    D_800625A0->flags->list_label_shown[fit] = 1;
                    D_800625A0->flags->scroll_shown = 1;
                    last_top = 0xFF;
                    last_row = 0xFF;
                    D_800625A0->flags->marker_shown[0] = 1;
                }
                func_801CC4DC();
                confirm = 0xFF;
            } else {
                func_801CB498(4);
            }
            break;
        case 5:
            D_800625A0->flags->panel_shown[5] = 0;
            running = 0;
            D_800625A0->flags->list_label_shown[fit] = 0;
            if (total != 0 && !fit) {
                D_800625A0->flags->unk5A = 0;
                D_800625A0->flags->panel_shown[2] = 0;
                D_800625A0->flags->panel_shown[3] = 0;
                D_800625A0->flags->scroll_shown = 0;
                D_800625A0->flags->marker_shown[0] = 0;
                func_801CC31C(0);
                if (!func_801CCC18(0x8C, 0xFF, 1)) {
                    running = 1;
                    D_800625A0->flags->panel_shown[2] = 1;
                    D_800625A0->flags->panel_shown[3] = 1;
                    D_800625A0->flags->panel_shown[5] = 1;
                    D_800625A0->flags->list_label_shown[0] = 1;
                    D_800625A0->flags->scroll_shown = 1;
                    last_top = 0xFF;
                    last_row = 0xFF;
                    D_800625A0->flags->marker_shown[0] = 1;
                }
                func_801CC4DC();
            }
            break;
        case 1:
            row++;
            if (row >= 8) {
                row = 7;
                top++;
                if (count - 8 < top) {
                    top--;
                }
            }
            break;
        case 3:
            row--;
            if (row < 0) {
                top--;
                row = 0;
                if (top < 0) {
                    top = 0;
                }
            }
            break;
        case 0:
            if (dims[row] != 0 && !fit &&
                D_800625A0->details->amounts[top + row] + (held_next = D_801D904C + 1) < 100) {
                total += price;
                new_gold -= price;
                redraw = 1;
                D_800625A0->details->amounts[top + row]++;
            }
            break;
        case 2:
            if (!fit && D_800625A0->details->amounts[top + row] != 0) {
                total -= price;
                new_gold += price;
                redraw = 1;
                D_800625A0->details->amounts[top + row] -= 1;
            }
            break;
        }
    }
    D_800625A0->details->label45B0_shown = 0;
    func_801C9054(5);
    func_801CCEBC(2, D_800625A0->flags->list_label_shown);
    func_801D0EC8(1);
    return 1;
}

/*
 * Refuel and repair the edited gear. Fuel costs 10 gold per 100 missing (at
 * least 10); with too little gold, buy what the gold covers. Repairs are free
 * and come with any purchase. A full tank can be repaired without buying fuel.
 */
void func_801D5398(void) {
    u8 message;
    u8 mode;
    u8 confirm;
    u8 done;
    u16 units;
    u32 price;

    done = 0;
    message = 0xA6;
    mode = 1;
    confirm = 1;
    if (D_8006DFAC[D_801D9084].max_fuel == D_8006DFAC[D_801D9084].fuel) {
        message = 0xB5;
        if (D_8006DFAC[D_801D9084].hp == D_8006DFAC[D_801D9084].max_hp) {
            message = 0xB8;
            mode = 0;
            confirm = 0;
        } else {
            mode = 2;
        }
    }
    units = (D_8006DFAC[D_801D9084].max_fuel - D_8006DFAC[D_801D9084].fuel) / 100;
    if (units == 0) {
        units = 1;
    }
    price = units * 10;
    func_801C90E0(5, 0xA2, 0xA6, 0x60, 0x14, 0, 1, 4, 0);
    func_801D0D4C(D_8006D634.gold);
    if (mode != 0) {
        func_801D0C20(price, 1);
    }
    func_801CC1C4();
    func_801CC31C(0);
    if (func_801CCC18(message, 0xFF, confirm) != 0) {
        D_800625A0->flags->unk5B = 0;
        switch (mode) {
        case 1:
            if (D_8006D634.gold < price) {
                if (func_801CCC18(0xA9, 0xFF, 1) != 0) {
                    if (D_8006D634.gold != 0) {
                        done = 1;
                    }
                    price = D_8006D634.gold / 10;
                    D_8006DFAC[D_801D9084].fuel += price * 100;
                    D_8006D634.gold %= 10;
                    if (D_8006DFAC[D_801D9084].max_fuel < D_8006DFAC[D_801D9084].fuel) {
                        D_8006DFAC[D_801D9084].fuel = D_8006DFAC[D_801D9084].max_fuel;
                    }
                }
            } else {
                D_8006D634.gold -= price;
                D_8006DFAC[D_801D9084].fuel = D_8006DFAC[D_801D9084].max_fuel;
                done = 1;
            }
            break;
        case 2:
            done = 1;
            break;
        }
    }
    if (done) {
        D_8006DFAC[D_801D9084].hp = D_8006DFAC[D_801D9084].max_hp;
        func_801CB498(0xD1);
    }
    func_801CC4DC(0);
    D_800625A0->flags->price_shown = 0;
    func_801C9054(5);
}

/* Leave the gear list: hide the cursor and labels and the shop list packets. */
u8 func_801D573C(void) {
    D_800625A0->flags->unk4 = 0;
    D_800625A0->flags->cursor_shown = 0;
    func_801CCEBC(4, D_800625A0->flags->list_label_shown);
    func_801D0EC8(0);
    return 2;
}

/* Redraw the current gear list: parts (0-2) or the fourth list. */
void func_801D57A8(void) {
    switch (D_800625A0->list_cursor) {
    case 0:
    case 1:
    case 2:
        func_801D498C(0, 1);
        func_801D6150((GearTable *)D_800625A0->resources, D_801D9084);
        break;
    case 3:
        func_801D5398();
        break;
    }
}

/*
 * The gear parts list of the current command (the fourth command starts at
 * its fourth entry, when func_801D573C allows it): move the cursor, open the
 * gear view for the chosen list (4), page the members (9, 10), leave (5).
 */
u8 func_801D5828(void) {
    u8 running;
    u8 first;
    u8 base;

    running = 1;
    first = 1;
    base = 0;
    if (D_800625A0->top_cursor == 3 && !func_801D573C()) {
        return 1;
    }
    D_800625A0->list_cursor = 0;
    D_800625A0->unk339 = 0xFF;
    if (D_800625A0->top_cursor == 3) {
        D_800625A0->list_cursor = 3;
        base = 4;
    }
    while (running) {
        func_801CC1C4();
        if (D_800625A0->view_motion == 0 && D_801D6FD8 >= 2) {
            D_800625A0->flags->marks_b_shown = 1;
        }
        if (first) {
            func_801CCE90(4, D_800625A0->list_labels, &D_801D6A24[base], D_800625A0->flags->list_label_shown);
            func_801CD564(0);
            first = 0;
        }
        if (D_800625A0->list_cursor != D_800625A0->unk339) {
            func_801CCEE8(4, D_800625A0->list_labels, &D_801D6A24[base], &D_801D6A40[base],
                          D_800625A0->flags->list_label_shown, D_800625A0->list_cursor, 4, 0);
            func_801CDA0C(0);
            D_800625A0->unk339 = D_800625A0->list_cursor;
        }
        if (D_800625A0->unk454->lamp_state[0] == 0) {
            D_800625A0->flags->gear_parts_shown = 1;
            D_800625A0->flags->gear_shown = 0;
        }
        switch (D_800625A0->input) {
        case 4:
            D_800625A0->flags->unk4 = 0;
            D_800625A0->flags->cursor_shown = 0;
            D_800625A0->flags->lists_shown = 0;
            D_800625A0->flags->gear_parts_shown = 0;
            func_801CCEBC(4, D_800625A0->flags->list_label_shown);
            func_801CB498(2);
            func_801CFC60();
            D_800625A0->flags->marks_b_shown = 0;
            switch (D_800625A0->top_cursor) {
            case 1:
                func_801D2804(1);
                break;
            case 2:
                func_801D498C(1, 0);
                break;
            case 3:
                func_801D57A8();
                break;
            }
            func_801CFF18();
            func_801CC1C4();
            func_801CF448();
            func_801CC1C4();
            D_800625A0->flags->unk4 = 1;
            D_800625A0->flags->cursor_shown = 1;
            D_800625A0->flags->lists_shown = 1;
            func_801CCE90(4, D_800625A0->list_labels, &D_801D6A24[base], D_800625A0->flags->list_label_shown);
            D_800625A0->unk339 = 0xFF;
            break;
        case 5:
            running = 0;
            break;
        case 1:
            if (D_800625A0->list_cursor != 0) {
                D_800625A0->list_cursor--;
            } else {
                D_800625A0->list_cursor = D_800625A0->list_count - 1;
            }
            break;
        case 3:
            if (++D_800625A0->list_cursor >= D_800625A0->list_count) {
                D_800625A0->list_cursor = 0;
            }
            break;
        case 9:
            func_801D0398(0);
            break;
        case 10:
            func_801D0398(1);
            break;
        }
    }
    D_800625A0->flags->unk4 = 0;
    D_800625A0->flags->cursor_shown = 0;
    func_801CCEBC(4, D_800625A0->flags->list_label_shown);
    return 1;
}

/* Set up the Gear model: the model code, its light, both large ordering tables and the two model blocks, then show the first present member's gear. */
void func_801D5D38(void) {
    s32 i;

    i = 0;
    func_801E738C(0x40);
    D_800625A0->model_light.direction.vx = 0x546;
    D_800625A0->model_light.direction.vy = -0xE39;
    D_800625A0->model_light.direction.vz = 0x546;
    D_800625A0->model_light.direction.pad = 0;
    D_800625A0->model_light.unk8[0] = 0;
    D_800625A0->model_light.unk8[1] = 0;
    D_800625A0->model_light.unk8[2] = 0;
    D_800625A0->model_light.unk8[3] = 0;
    D_800625A0->model_light.unk8[4] = 0;
    D_800625A0->model_light.color.m[0][0] = 0x600;
    D_800625A0->model_light.color.m[0][1] = 0;
    D_800625A0->model_light.color.m[0][2] = 0;
    D_800625A0->model_light.color.m[1][0] = 0x600;
    D_800625A0->model_light.color.m[1][1] = 0;
    D_800625A0->model_light.color.m[1][2] = 0;
    D_800625A0->model_light.color.m[2][0] = 0x600;
    D_800625A0->model_light.color.m[2][1] = 0;
    D_800625A0->model_light.color.m[2][2] = 0;
    D_801E8644 = &D_800625A0->model_light.color;
    SetBackColor(0x3C, 0x3C, 0x3C);
    D_800625A0->envs[0].ot_big = D_8005A4AC[0];
    D_800625A0->envs[1].ot_big = D_8005A4AC[1];
    D_800625A0->model_parts[0] = func_80031BDC(sizeof(ModelParts), 0);
    bzero(D_800625A0->model_parts[0], sizeof(ModelParts));
    D_800625A0->model_parts[1] = func_80031BDC(sizeof(ModelParts), 0);
    bzero(D_800625A0->model_parts[1], sizeof(ModelParts));
    while (D_800625A0->member_present[i] == 0) {
        i++;
    }
    D_801D9084 = D_8006D634.characters[i].unkA0;
    func_801CF9BC(D_801D9084, 1);
    func_801D0348();
}

/* Close the gear model once the view has stopped moving, and release its two blocks. */
void func_801D5EB8(void) {
    while (D_800625A0->view_motion != 0) {
        func_801CC1C4();
    }
    D_801D697C = 0;
    D_800625A0->flags->model_shown = 0;
    func_801CC1C4();
    func_801E7FD4();
    D_800625A0->model_parts[0]->unk12 = 0;
    D_800625A0->model_parts[1]->unk12 = 0;
    func_800320E8(D_800625A0->model_parts[0]);
    func_800320E8(D_800625A0->model_parts[1]);
}

/*
 * Summarise gear `id` for the parts screen: its values plus its parts' and
 * its pilot's bonuses.
 */
void func_801D5F94(GearTable *table, u8 id) {
    Gear *gear;
    Character *pilot;
    s32 bonus;

    if (D_8006D634.unk22B6 & 0x1000) {
        D_801D70FD = 10;
    }
    gear = &D_8006D634.gears[id];
    pilot = &D_8006D634.characters[D_801D70F4[id]];
    table->unk9C = gear->hp;
    table->unkA0 = gear->max_hp;
    table->unkA4 = gear->unk70 + gear->unk40;
    table->unkA6 = pilot->bonus[4] + pilot->base[4] + gear->unk42 + gear->unk72;
    table->unkA8 = gear->unk68 + gear->unk44;
    table->unkAA = gear->unk6A;
    table->unkAC = gear->fuel;
    table->unkAE = gear->max_fuel;
    bonus = gear->unk3C * (gear->unk74 + gear->unk55[1]);
    if (id == 5 || id == 13) {
        table->unkB0 = (gear->unk12 + gear->unk22) * 6 / 10 + bonus;
    } else {
        table->unkB0 = gear->unk12 + bonus;
    }
    table->unkB2 = gear->unk9F + gear->unk4D;
    table->unkB3 = gear->unk98 - gear->unk4A;
    table->unkB4 = gear->unk9E;
    table->unkB5 = gear->unk9D;
    table->unkB6 = gear->unk9C;
}

/* Rebuild gear `id`'s derived values. */
void func_801D6150(GearTable *table, u8 id) {
    func_801D61B8(table, id);
    func_801D62A4(table, id);
    func_801D6250(table, id);
    func_801D6334(table, id);
    func_801D6738(table, id);
}

/* Copy gear `id`'s values from its +2 entry of the table's 18h-byte records, capping +60. */
void func_801D61B8(GearTable *table, u8 id) {
    Gear *gear;
    GearRecord18 *record;

    gear = &D_8006DFAC[id];
    record = table->records18;
    record += gear->unk2;
    gear->max_hp = record->unk4;
    gear->unk68 = record->unk8;
    gear->unk98 = record->unk14;
    gear->unk9E = record->unk15;
    gear->unk9D = record->unk16;
    gear->unk9F = record->unk17;
    if (gear->max_hp < gear->hp) {
        gear->hp = gear->max_hp;
    }
}

/* Copy gear `id`'s two words from its entry (+8) of the table's 14h-byte records. */
void func_801D6250(GearTable *table, u8 id) {
    Gear *gear;
    GearEntry *entry;

    gear = &D_8006DFAC[id];
    entry = table->entries;
    entry += gear->unk8;
    gear->unk70 = entry->unk8;
    gear->unk72 = entry->unkA;
}

/* Copy gear `id`'s values from its +3 entry of the table's 10h-byte records, capping +38. */
void func_801D62A4(GearTable *table, u8 id) {
    Gear *gear;
    GearRecord10 *record;
    u16 limit;

    gear = &D_8006DFAC[id];
    record = table->records10;
    limit = gear->fuel;
    record += gear->unk3;
    gear->max_fuel = record->unk6;
    gear->unk3C = record->unkC;
    gear->unk3D = record->unkD;
    gear->unk3E = record->unkE;
    gear->unk3F = record->unkE;
    if (gear->max_fuel < limit) {
        gear->fuel = gear->max_fuel;
    }
}

/* Sum gear `id`'s three parts into its derived values and effect bits, and update its pilot's ability bits. */
void func_801D6334(GearTable *table, u8 id) {
    Gear *gear;
    GearPart *part;
    u16 *abilities;
    u16 *status;
    u8 i;
    u8 k;

    gear = &D_8006D634.gears[id];
    abilities = &D_8006D634.pilots[D_801D70F4[id]].flags;
    status = &D_8006D634.pilots[D_801D70F4[id]].unk16;
    gear->unk40 = 0;
    gear->unk42 = 0;
    gear->unk44 = 0;
    gear->unk48 = 0;
    gear->unk4C = 0;
    gear->unk4D = 0;
    gear->unk4E = 0;
    gear->unk4F = 0;
    gear->unk6E = 0;
    for (i = 0; i < 16; i++) {
        gear->unk88[i] = 0;
    }
    for (i = 0; i < 4; i++) {
        gear->unk50[i] = 0;
    }
    for (i = 0; i < 3; i++) {
        gear->unk55[i] = 0;
    }
    gear->unk54 = 0;
    gear->unk7E = 0;
    gear->unk82 = 0;
    gear->unk86 &= 0xF000;
    *abilities &= 0xDB7F;
    for (k = 0; k < 3; k++) {
        part = table->parts;
        part += gear->unk9[k];
        gear->unk40 += part->unkD;
        gear->unk42 += part->unkE;
        gear->unk44 += part->unk6;
        gear->unk4C += part->unk18;
        gear->unk4D += part->unk14;
        for (i = 0; i < 4; i++) {
            gear->unk50[i] += part->unk10[i];
        }
        switch (part->unk15) {
        case 1:
            gear->unk7E |= part->unk16;
            break;
        case 2:
            gear->unk82 |= part->unk16;
            break;
        case 3:
            gear->unk86 |= part->unk16;
            break;
        case 4:
            for (i = 0; i < 16; i++) {
                gear->unk6E |= part->unk16;
                if (part->unk16 & (0x8000 >> i)) {
                    gear->unk88[i] += part->unk1A;
                }
            }
            break;
        case 5:
            gear->unk4F += part->unk16;
            break;
        case 6:
            if ((*abilities & 0x1000) && (*abilities & 0x800)) {
                *abilities |= 0x400;
            }
            break;
        case 7:
            if ((*abilities & 0x200) && (*abilities & 0x100)) {
                *abilities |= 0x80;
            }
            break;
        case 8:
            if ((*abilities & 0x40) && (*abilities & 0x20)) {
                *abilities |= 0x10;
            }
            break;
        case 9:
            gear->unk48 |= part->unk16;
            /* fallthrough */
        case 10:
            gear->unk55[1] += part->unk16;
            break;
        case 11:
            gear->unk55[2] += part->unk16;
            break;
        }
    }
    gear->unk4A = func_801D690C(id);
    if (gear->unk4F != 0) {
        *status |= 0x8000;
    } else if (id == D_8006D634.characters[D_801D70F4[id]].unkA0) {
        *status &= 0x7FFF;
    }
}

/* Copy gear `id`'s weapon values from the weapon table; gear 5 and 13 carry three weapons. */
void func_801D6738(GearTable *table, u8 id) {
    Gear *gear;
    GearWeapon *weapon;

    gear = &D_8006D634.gears[id];
    weapon = table->weapons;
    weapon += gear->unkC[0];
    gear->unk12 = weapon->unkE;
    gear->unk10 = weapon->unk12;
    gear->unk13 = weapon->unk10;
    gear->unk14 = weapon->unk11;
    gear->unk5C = weapon->unk0;
    gear->unk5D = weapon->unk1;
    gear->unk5E = weapon->unk2;
    gear->unk5F = weapon->unk3;
    if (gear->unk14 == 100) {
        gear->unk86 &= 0xFFF;
        gear->unk86 |= gear->unk10;
    }
    if (id == 5 || id == 13) {
        weapon = table->weapons;
        weapon += gear->unk4[0];
        gear->unk12 = weapon->unkE;
        gear->unk10 = weapon->unk12;
        gear->unk13 = weapon->unk10;
        gear->unk14 = weapon->unk11;
        gear->unk5D = weapon->unk1;
        weapon = table->weapons;
        weapon += gear->unk4[1];
        gear->unk1A = weapon->unkE;
        gear->unk18 = weapon->unk12;
        gear->unk1B = weapon->unk10;
        gear->unk1C = weapon->unk11;
        gear->unk5E = weapon->unk2;
        weapon = table->weapons;
        weapon += gear->unk4[3];
        gear->unk22 = weapon->unkE;
        gear->unk20 = weapon->unk12;
        gear->unk23 = weapon->unk10;
        gear->unk24 = weapon->unk11;
        gear->unk5F = weapon->unk3;
    }
}

/* Half of (gear `id`'s +44 / 120 less its +75), not below zero. */
u8 func_801D690C(u8 id) {
    Gear *gear;
    s16 value;

    gear = &D_8006DFAC[id];
    value = ((u16)(gear->unk44 / 120) - gear->unk75) / 2;
    if (value < 0) {
        value = 0;
    }
    return value;
}
