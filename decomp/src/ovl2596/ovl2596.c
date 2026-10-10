/* Post-battle module (Disc 1 slot 2596, directory 10 file 4), loaded at
 * 801de000 by the battle overlay's 80070f40 (the read at 800715d8). It
 * builds and queues the victory result screens (member cards, summary and
 * spoils windows), distributes experience and levels, grants skills, gold and
 * drops, and writes the party back to the game data. The battle overlay calls
 * 801de594 (queue the screens' primitives) and 801e252c.
 *
 * The whole image is this one unit: rodata 801DE000-801DE048, text
 * 801DE048-801E44C0, data 801E44C0-801E44CC and its commons to 801E4500.
 * Compiled with GCC 2.6.3: only it reproduces the (index + base) operand
 * order of this unit's address arithmetic (e.g. 801de5c4, 801de1c4). */
#include "battle_results.h"

/* The module's data opens with D_801E44C0 (the fanfare flag, u8 0): the
 * padding after it holds a byte (0x04) that nothing reads, which a C
 * definition cannot leave, so it stays original data. */
INCLUDE_ORIGINAL(".data", D_801E44C0, 0x801E44C0, 4);
extern u8 D_801E44C0; /* the result fanfare has started */
GameData *D_801E44C4 = &game_data;
BattleWork *D_801E44C8 = &battle_work_area;
/* The module's uninitialized variables (the level gauge animation, the
 * growth data and the record being processed, the experience pools), zero
 * in the file: commons, which the original linker allocated each in a slot
 * of whole words (decomp/Makefile). */
s32 D_801E44CC;               /* gauge: start value */
s32 D_801E44D0;               /* end value */
s32 D_801E44D4;               /* distance */
s32 D_801E44D8;               /* start length */
s32 D_801E44DC;               /* distance length */
u8 D_801E44E0;                /* bar colour */
u8 D_801E44E4;                /* arrow glyph */
GrowthFile *D_801E44E8;       /* the growth data file */
CharacterRecord *D_801E44EC;  /* the record being processed: a combatant's or the game data's */
u32 D_801E44F0;               /* experience pool for level A */
u32 D_801E44F4;               /* and level B */
u8 D_801E44F8[3][2];          /* each slot's levels A and B before the experience */

/* Queue every member card's glyph runs while the cards are shown. */
void func_801DE048(void) {
    s32 i;

    if (battle_ui->showCards != 0) {
        for (i = 0; i < 3; i++) {
            battle_add_prims_to_ot(battle_member_cards[i]->portrait, battle_member_cards[i]->runs[0].count, battle_member_cards[i]->runs[0].buffer);
            battle_add_prims_to_ot(battle_member_cards[i]->labels, battle_member_cards[i]->runs[1].count, battle_member_cards[i]->runs[1].buffer);
            battle_add_prims_to_ot(battle_member_cards[i]->field960, battle_member_cards[i]->runs[4].count, battle_member_cards[i]->runs[4].buffer);
            battle_add_prims_to_ot(battle_member_cards[i]->fieldB40, battle_member_cards[i]->runs[6].count, battle_member_cards[i]->runs[6].buffer);
            battle_add_prims_to_ot(battle_member_cards[i]->fieldA50, battle_member_cards[i]->runs[5].count, battle_member_cards[i]->runs[5].buffer);
            battle_add_prims_to_ot(battle_member_cards[i]->fieldBE0, battle_member_cards[i]->runs[7].count, battle_member_cards[i]->runs[7].buffer);
            battle_add_prims_to_ot(battle_member_cards[i]->field780, battle_member_cards[i]->runs[2].count, battle_member_cards[i]->runs[2].buffer);
            battle_add_prims_to_ot(battle_member_cards[i]->field870, battle_member_cards[i]->runs[3].count, battle_member_cards[i]->runs[3].buffer);
            battle_add_prims_to_ot(battle_member_cards[i]->fieldC80, battle_member_cards[i]->runs[8].count, battle_member_cards[i]->runs[8].buffer);
            battle_add_prims_to_ot(battle_member_cards[i]->fieldF00, battle_member_cards[i]->runs[9].count, battle_member_cards[i]->runs[9].buffer);
            battle_add_prims_to_ot(battle_member_cards[i]->field1180, battle_member_cards[i]->runs[10].count, battle_member_cards[i]->runs[10].buffer);
            battle_add_prims_to_ot(battle_member_cards[i]->field13B0, battle_member_cards[i]->runs[11].count, battle_member_cards[i]->runs[11].buffer);
        }
    }
}

/* Queue the summary window's glyphs and bars, and the 8F panel. */
void func_801DE1C4(void) {
    s32 i;

    if (battle_ui->showSummary != 0) {
        battle_add_prims_to_ot(battle_summary_window_prims->title[0], battle_summary_window_prims->runs[0].count, battle_summary_window_prims->runs[0].buffer);
        battle_add_prims_to_ot(battle_summary_window_prims->text, battle_summary_window_prims->runs[1].count, battle_summary_window_prims->runs[1].buffer);
        battle_add_prims_to_ot(battle_summary_window_prims->glyphs1630, battle_summary_window_prims->runs[2].count, battle_summary_window_prims->runs[2].buffer);
        battle_add_prims_to_ot(battle_summary_window_prims->glyphs1720, battle_summary_window_prims->runs[4].count, battle_summary_window_prims->runs[4].buffer);
        battle_add_prims_to_ot(battle_summary_window_prims->glyphs17C0, battle_summary_window_prims->runs[3].count, battle_summary_window_prims->runs[3].buffer);
        battle_add_prims_to_ot(battle_summary_window_prims->glyphs1900, battle_summary_window_prims->runs[5].count, battle_summary_window_prims->runs[5].buffer);
        for (i = 0; i < 7; i++) {
            AddPrim(battle_area.ot + 1, &battle_summary_window_prims->barB[i][battle_summary_window_prims->barBuffer[i]]);
            AddPrim(battle_area.ot + 1, &battle_summary_window_prims->barA[i][battle_summary_window_prims->barBuffer[i]]);
            battle_add_prims_to_ot(battle_summary_window_prims->rowA[i], battle_summary_window_prims->rowACount[i], battle_summary_window_prims->rowABuffer[i]);
            battle_add_prims_to_ot(battle_summary_window_prims->rowB[i], battle_summary_window_prims->rowBCount[i], battle_summary_window_prims->rowBBuffer[i]);
        }
    }
    i = 0;
    if (battle_ui->showSkill != 0) {
        battle_add_prims_to_ot(battle_summary_window_prims->title[0], battle_summary_window_prims->runs[0].count, battle_summary_window_prims->runs[0].buffer);
        for (; i < 2; i++) {
            AddPrim(battle_area.ot + 1, &battle_summary_window_prims->glyphs34B0[i][battle_summary_window_prims->buffer34B0[i]]);
        }
    }
}

/* Queue the spoils window's glyphs and item list. */
void func_801DE408(void) {
    s32 i;

    if (battle_ui->showSpoils != 0) {
        battle_add_prims_to_ot(battle_summary_window_prims->glyphs2D30[0], 7, battle_summary_window_prims->buffer2D30);
        battle_add_prims_to_ot(battle_summary_window_prims->glyphs2F60[0], battle_summary_window_prims->run2F60.count, battle_summary_window_prims->run2F60.buffer);
        battle_add_prims_to_ot(battle_summary_window_prims->glyphs3140[0], battle_summary_window_prims->run3140.count, battle_summary_window_prims->run3140.buffer);
        for (i = 0; i < battle_summary_window_prims->listCount; i++) {
            AddPrim(battle_area.ot + 1, &battle_summary_window_prims->listA[i][battle_summary_window_prims->listBuffer]);
            AddPrim(battle_area.ot + 1, &battle_summary_window_prims->listB[i][battle_summary_window_prims->listBuffer]);
        }
        for (i = 0; i < 2; i++) {
            AddPrim(battle_area.ot + 1, &battle_summary_window_prims->glyphs3410[i][battle_summary_window_prims->buffer3410]);
        }
    }
}

/* Queue the result screens' primitives. */
void func_801DE594(void) {
    func_801DE048();
    func_801DE1C4();
    func_801DE408();
}

/* Build each present member's portrait glyphs. */
void func_801DE5C4(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        battle_member_cards[i]->runs[0].count = 0;
        if (battle_area.slots[i].field2 != 0x7F) {
            battle_member_cards[i]->runs[0].count += battle_build_glyph(i + 0xFC, &battle_member_cards[i]->portrait[battle_member_cards[i]->runs[0].count * 2], 0x20, i * 0x20 + 0x24);
        }
        battle_member_cards[i]->runs[0].buffer = battle_area.buffer;
    }
}

/* Build each present member's card labels; with the first card's flag, add
 * the two marker glyphs, shaded red and green. */
void func_801DE69C(void) {
    s32 i;
    s32 j;

    for (i = 0; i < 3; i++) {
        battle_member_cards[i]->runs[1].count = 0;
        if (battle_area.slots[i].field2 != 0x7F) {
            for (j = 0; j < 18; j++) {
                battle_member_cards[i]->runs[1].count += battle_build_glyph(battle_member_card_label_glyphs[j], &battle_member_cards[i]->labels[battle_member_cards[i]->runs[1].count * 2], battle_member_card_label_x[j], battle_member_card_label_y[j] + i * 0x20);
            }
            if (battle_member_cards[0]->secondValue != 0) {
                battle_member_cards[i]->runs[1].count += battle_build_glyph(0xE8, &battle_member_cards[i]->labels[battle_member_cards[i]->runs[1].count * 2], 0x88, i * 0x20 + 0x20);
                battle_member_cards[i]->runs[1].count += battle_build_glyph(0xE9, &battle_member_cards[i]->labels[battle_member_cards[i]->runs[1].count * 2], 0x88, i * 0x20 + 0x28);
                SetShadeTex(&battle_member_cards[i]->labels[(battle_member_cards[i]->runs[1].count - 2) * 2 + battle_area.buffer], 0);
                setRGB0(&battle_member_cards[i]->labels[battle_member_cards[i]->runs[1].count * 2 + battle_area.buffer] - 4, 0x80, 0x40, 0x40);
                SetShadeTex(&battle_member_cards[i]->labels[(battle_member_cards[i]->runs[1].count - 1) * 2 + battle_area.buffer], 0);
                setRGB0(&battle_member_cards[i]->labels[battle_member_cards[i]->runs[1].count * 2 + battle_area.buffer] - 2, 0x40, 0x80, 0x40);
            }
        }
        battle_member_cards[i]->runs[1].buffer = battle_area.buffer;
    }
}

/* Build each present member's four numbers as digit glyphs: two stats (three
 * and two digits) and two further values beside them. */
void func_801DEA18(void) {
    s32 i;
    s32 j;
    s32 n;
    s32 digit;

    for (i = 0; i < 3; i++) {
        battle_member_cards[i]->runs[4].count = 0;
        battle_member_cards[i]->runs[6].count = 0;
        battle_member_cards[i]->runs[5].count = 0;
        battle_member_cards[i]->runs[7].count = 0;
        if (battle_area.slots[i].field2 != 0x7F) {
            battle_split_decimal_digits(battle_work_area.records[i].pilot.hp);
            for (j = 0; j < 3; j++) {
                n = j + 9;
                digit = battle_decimal_digits_minus_3[n];
                if (digit != 0xFF) {
                    battle_member_cards[i]->runs[4].count += battle_build_glyph(digit, &battle_member_cards[i]->field960[battle_member_cards[i]->runs[4].count * 2], j * 8 + 0x48, i * 0x20 + 0x20);
                }
            }
            battle_member_cards[i]->runs[4].buffer = battle_area.buffer;
            battle_split_decimal_digits(battle_work_area.records[i].pilot.ep);
            for (j = 0; j < 2; j++) {
                n = j + 10;
                digit = battle_decimal_digits_minus_3[n];
                if (digit != 0xFF) {
                    battle_member_cards[i]->runs[6].count += battle_build_glyph(digit, &battle_member_cards[i]->fieldB40[battle_member_cards[i]->runs[6].count * 2], j * 8 + 0x50, i * 0x20 + 0x28);
                }
            }
            battle_member_cards[i]->runs[6].buffer = battle_area.buffer;
            battle_split_decimal_digits(battle_work_area.savedMax[i][0]);
            for (j = 0; j < 3; j++) {
                n = j + 13;
                digit = battle_decimal_digits_minus_7[n];
                if (digit != 0xFF) {
                    battle_member_cards[i]->runs[5].count += battle_build_glyph(digit, &battle_member_cards[i]->fieldA50[battle_member_cards[i]->runs[5].count * 2], j * 8 + 0x68, i * 0x20 + 0x20);
                }
            }
            battle_member_cards[i]->runs[5].buffer = battle_area.buffer;
            battle_split_decimal_digits(battle_work_area.savedMax[i][1]);
            for (j = 0; j < 2; j++) {
                n = j + 14;
                digit = battle_decimal_digits_minus_7[n];
                if (digit != 0xFF) {
                    battle_member_cards[i]->runs[7].count += battle_build_glyph(digit, &battle_member_cards[i]->fieldBE0[battle_member_cards[i]->runs[7].count * 2], j * 8 + 0x70, i * 0x20 + 0x28);
                }
            }
            battle_member_cards[i]->runs[7].buffer = battle_area.buffer;
        }
    }
}

/* Build each present member's level glyphs, from the battle slots or (with
 * fromGameData) the game data; with the first card's flag also the second
 * level, and shade the two numbers red and green. */
void func_801DEDC0(u8 fromGameData) {
    s32 i;
    s32 j;
    s32 n;
    s32 digit;

    for (i = 0; i < 3; i++) {
        battle_member_cards[i]->runs[2].count = 0;
        battle_member_cards[i]->runs[3].count = 0;
        if (battle_area.slots[i].field2 != 0x7F) {
            if (fromGameData == 0) {
                battle_split_decimal_digits(battle_slot_levels[i].level);
            } else {
                battle_split_decimal_digits(game_data.characters[battle_party_character_ids[i]].level);
            }
            for (j = 0; j < 3; j++) {
                n = j + 18;
                digit = battle_finished_motion_count[n];
                if (digit != 0xFF) {
                    battle_member_cards[i]->runs[2].count += battle_build_glyph(digit, &battle_member_cards[i]->field780[battle_member_cards[i]->runs[2].count * 2], j * 8 + 0x90, i * 0x20 + 0x20);
                }
            }
            battle_member_cards[i]->runs[2].buffer = battle_area.buffer;
            if (battle_member_cards[0]->secondValue != 0) {
                if (fromGameData == 0) {
                    battle_split_decimal_digits(battle_slot_levels[i].level2);
                } else {
                    battle_split_decimal_digits(game_data.characters[battle_party_character_ids[i]].level2);
                }
                for (j = 0; j < 3; j++) {
                    n = j + 18;
                    digit = battle_finished_motion_count[n];
                    if (digit != 0xFF) {
                        battle_member_cards[i]->runs[3].count += battle_build_glyph(digit, &battle_member_cards[i]->field870[battle_member_cards[i]->runs[3].count * 2], j * 8 + 0x90, i * 0x20 + 0x28);
                    }
                }
                battle_member_cards[i]->runs[3].buffer = battle_area.buffer;
                for (j = 0; j < battle_member_cards[i]->runs[2].count; j++) {
                    SetShadeTex(&battle_member_cards[i]->field780[j * 2 + battle_area.buffer], 0);
                    setRGB0(&battle_member_cards[i]->field780[j * 2 + battle_area.buffer], 0x80, 0x40, 0x40);
                }
                for (j = 0; j < battle_member_cards[i]->runs[3].count; j++) {
                    SetShadeTex(&battle_member_cards[i]->field870[j * 2 + battle_area.buffer], 0);
                    setRGB0(&battle_member_cards[i]->field870[j * 2 + battle_area.buffer], 0x40, 0x80, 0x40);
                }
            }
        }
    }
}

/* Build each present member's first eight-digit number (and with the first
 * card's flag the second) as glyphs. */
void func_801DF270(void) {
    s32 i;
    s32 j;
    s32 n;
    s32 digit;

    for (i = 0; i < 3; i++) {
        battle_member_cards[i]->runs[8].count = 0;
        battle_member_cards[i]->runs[9].count = 0;
        if (battle_area.slots[i].field2 != 0x7F) {
            battle_split_decimal_digits(battle_work_area.expTotals[i][0]);
            for (j = 0; j < 8; j++) {
                n = j + 22;
                digit = battle_decimal_digits_minus_21[n];
                if (digit != 0xFF) {
                    battle_member_cards[i]->runs[8].count += battle_build_glyph(digit, &battle_member_cards[i]->fieldC80[battle_member_cards[i]->runs[8].count * 2], j * 8 + 0xB0, i * 0x20 + 0x20);
                }
            }
            battle_member_cards[i]->runs[8].buffer = battle_area.buffer;
            if (battle_member_cards[0]->secondValue != 0) {
                battle_split_decimal_digits(battle_work_area.expTotals[i][1]);
                for (j = 0; j < 8; j++) {
                    n = j + 22;
                    digit = battle_decimal_digits_minus_21[n];
                    if (digit != 0xFF) {
                        battle_member_cards[i]->runs[9].count += battle_build_glyph(digit, &battle_member_cards[i]->fieldF00[battle_member_cards[i]->runs[9].count * 2], j * 8 + 0xB0, i * 0x20 + 0x28);
                    }
                }
                battle_member_cards[i]->runs[9].buffer = battle_area.buffer;
            }
        }
    }
}

/* Build each present member's seven-digit numbers as glyphs, like 801df270. */
void func_801DF4C0(void) {
    s32 i;
    s32 j;
    s32 n;
    s32 digit;

    for (i = 0; i < 3; i++) {
        battle_member_cards[i]->runs[10].count = 0;
        battle_member_cards[i]->runs[11].count = 0;
        if (battle_area.slots[i].field2 != 0x7F) {
            battle_split_decimal_digits(battle_work_area.toCount[i][0]);
            for (j = 0; j < 7; j++) {
                n = j + 31;
                digit = battle_decimal_digits_minus_29[n];
                if (digit != 0xFF) {
                    battle_member_cards[i]->runs[10].count += battle_build_glyph(digit, &battle_member_cards[i]->field1180[battle_member_cards[i]->runs[10].count * 2], j * 8 + 0xF8, i * 0x20 + 0x20);
                }
            }
            battle_member_cards[i]->runs[10].buffer = battle_area.buffer;
            if (battle_member_cards[0]->secondValue != 0) {
                battle_split_decimal_digits(battle_work_area.toCount[i][1]);
                for (j = 0; j < 7; j++) {
                    n = j + 31;
                    digit = battle_decimal_digits_minus_29[n];
                    if (digit != 0xFF) {
                        battle_member_cards[i]->runs[11].count += battle_build_glyph(digit, &battle_member_cards[i]->field13B0[battle_member_cards[i]->runs[11].count * 2], j * 8 + 0xF8, i * 0x20 + 0x28);
                    }
                }
                battle_member_cards[i]->runs[11].buffer = battle_area.buffer;
            }
        }
    }
}

/* Set up a gauge bar's two primitives: a gradient from colour (0 pink,
 * 1 light green, 2 red, 3 blue) at the top to black. */
void func_801DF710(POLY_G4 *bar, u8 colour) {
    u8 rgb[3];
    s32 i;

    switch (colour) {
    case 0:
        rgb[0] = 0xFF;
        rgb[1] = 0x80;
        rgb[2] = 0x80;
        break;
    case 1:
        rgb[0] = 0x80;
        rgb[1] = 0xFF;
        rgb[2] = 0x80;
        break;
    case 2:
        rgb[0] = 0xFF;
        rgb[1] = 0;
        rgb[2] = 0;
        break;
    case 3:
        rgb[0] = 0;
        rgb[1] = 0;
        rgb[2] = 0xFF;
        break;
    }
    for (i = 0; i < 2; i++) {
        SetPolyG4(&bar[i]);
        bar[i].r0 = rgb[0];
        bar[i].g0 = rgb[1];
        bar[i].b0 = rgb[2];
        bar[i].r1 = rgb[0];
        bar[i].g1 = rgb[1];
        bar[i].b1 = rgb[2];
        bar[i].r2 = 0;
        bar[i].g2 = 0;
        bar[i].b2 = 0;
        bar[i].r3 = 0;
        bar[i].g3 = 0;
        bar[i].b3 = 0;
    }
}

/* Shade count glyph parts from the given draw buffer red (or blue). */
void func_801DF840(POLY_FT4 *prims, u8 blue, u8 count, u8 buffer) {
    u8 rgb[3];
    s32 i;

    rgb[1] = 0x40;
    if (blue == 0) {
        rgb[0] = 0x80;
        rgb[2] = 0x40;
    } else {
        rgb[0] = 0x40;
        rgb[2] = 0x80;
    }
    for (i = 0; i < count; i++) {
        SetShadeTex(&prims[i * 2 + buffer], 0);
        setRGB0(&prims[i * 2 + buffer], rgb[0], rgb[1], rgb[2]);
    }
}

/* Start the level gauge animation from one value to another out of max:
 * lengths on a 64-pixel scale, the colour and arrow for up or down. */
void func_801DF910(u8 from, u8 to, s32 max) {
    D_801E44CC = from;
    D_801E44D0 = to;
    D_801E44D4 = to - from;
    D_801E44D8 = from * 100 / max * 0x1900 / 10000;
    if (D_801E44D4 >= 0) {
        D_801E44E0 = 2;
        D_801E44E4 = 0xE3;
    } else {
        D_801E44E0 = 3;
        D_801E44E4 = 0xE5;
        D_801E44D4 = from - to;
    }
    D_801E44DC = D_801E44D4 * 100 / max * 0x1900 / 10000;
}

/* The highest of a slot's seven entries in both byte tables. */
s32 func_801DFA38(u8 slot) {
    u8 best = 0;
    s32 i;

    for (i = 0; i < 7; i++) {
        if (battle_work_area.savedStats[slot][i] >= best) {
            best = battle_work_area.savedStats[slot][i];
        }
        if (battle_work_area.resultStats[slot][i] >= best) {
            best = battle_work_area.resultStats[slot][i];
        }
    }
    return best;
}

/* Build the summary window: the member's portrait title and the 27 text
 * glyphs, shading the marked ones with their colour. */
void func_801DFAA8(u8 member) {
    s16 colours[6]; /* two RGB shading colours */
    s32 i;
    s32 start;
    s32 k;

    colours[0] = 0x80;
    colours[1] = 0x40;
    colours[2] = 0x40;
    colours[3] = 0x40;
    colours[4] = 0x40;
    colours[5] = 0x40;
    battle_summary_window_prims->runs[0].count = battle_build_glyph(member + 0xFC, battle_summary_window_prims->title[0], 0x3E, 0xA4);
    battle_summary_window_prims->runs[0].buffer = battle_area.buffer;
    battle_summary_window_prims->runs[1].count = 0;
    for (i = 0; i < 27; i++) {
        start = battle_summary_window_prims->runs[1].count;
        if (battle_summary_text_glyphs[i * 3] != 0xFF) {
            battle_summary_window_prims->runs[1].count += battle_build_glyph(battle_summary_text_glyphs[i * 3], &battle_summary_window_prims->text[start * 2], battle_summary_text_x[i], battle_summary_text_y[i]);
            if (battle_summary_text_glyphs[i * 3 + 1] != 0) {
                for (k = start; k < battle_summary_window_prims->runs[1].count; k++) {
                    SetShadeTex(&battle_summary_window_prims->text[k * 2 + battle_area.buffer], 0);
                    setRGB0(&battle_summary_window_prims->text[k * 2 + battle_area.buffer], colours[battle_summary_text_glyphs[i * 3 + 2] * 3],
                            colours[battle_summary_text_glyphs[i * 3 + 2] * 3 + 1], colours[battle_summary_text_glyphs[i * 3 + 2] * 3 + 2]);
                }
            }
        }
    }
    battle_summary_window_prims->runs[1].buffer = battle_area.buffer;
}

/* Build the summary's first member value (three digits); clear the other
 * summary number runs. */
void func_801DFD58(u8 member) {
    s32 j;
    s32 n;
    s32 digit;

    battle_summary_window_prims->runs[2].count = 0;
    battle_summary_window_prims->runs[4].count = 0;
    battle_summary_window_prims->runs[3].count = 0;
    battle_summary_window_prims->runs[5].count = 0;
    battle_split_decimal_digits(battle_work_area.savedMax[member][0]);
    for (j = 0; j < 3; j++) {
        n = j + 23;
        digit = battle_decimal_digits_minus_17[n];
        if (digit != 0xFF) {
            battle_summary_window_prims->runs[2].count += battle_build_glyph(digit, &battle_summary_window_prims->glyphs1630[battle_summary_window_prims->runs[2].count * 2], j * 8 + 0xB8, 0x80);
        }
    }
    battle_summary_window_prims->runs[2].buffer = battle_area.buffer;
}

/* Build the summary's second member value (two digits). */
void func_801DFE6C(u8 member) {
    s32 j;
    s32 n;
    s32 digit;

    battle_split_decimal_digits(battle_work_area.savedMax[member][1]);
    for (j = 0; j < 2; j++) {
        n = j + 24;
        digit = battle_decimal_digits_minus_17[n];
        if (digit != 0xFF) {
            battle_summary_window_prims->runs[4].count += battle_build_glyph(digit, &battle_summary_window_prims->glyphs1720[battle_summary_window_prims->runs[4].count * 2], j * 8 + 0xC0, 0x88);
        }
    }
    battle_summary_window_prims->runs[4].buffer = battle_area.buffer;
}

/* Build the summary's change of the member's first stat since the battle
 * began: an up or down arrow and the difference, shaded red. */
void func_801DFF50(u8 member) {
    s32 before;
    s32 after;
    s32 difference;
    s32 arrow;
    s32 j;
    s32 n;
    s32 digit;

    before = battle_work_area.savedMax[member][0];
    after = game_data.characters[battle_party_character_ids[member]].maxHp;
    difference = after - before;
    arrow = 0xE3;
    if (difference < 0) {
        arrow = 0xE5;
        difference = before - after;
    }
    if (difference != 0) {
        battle_summary_window_prims->runs[3].count = battle_build_glyph(arrow, battle_summary_window_prims->glyphs17C0, 0xD8, 0x80);
        battle_split_decimal_digits(difference);
        n = 0;
        for (j = 0; j < 3; j++) {
            digit = battle_decimal_digits_plus_6[j];
            if (digit != 0xFF) {
                battle_summary_window_prims->runs[3].count += battle_build_glyph(digit, &battle_summary_window_prims->glyphs17C0[battle_summary_window_prims->runs[3].count * 2], n * 8 + 0xE0, 0x80);
                n++;
            }
        }
        for (j = 0; j < battle_summary_window_prims->runs[3].count; j++) {
            SetShadeTex(&battle_summary_window_prims->glyphs17C0[j * 2 + battle_area.buffer], 0);
            setRGB0(&battle_summary_window_prims->glyphs17C0[j * 2 + battle_area.buffer], 0x80, 0x40, 0x40);
        }
        battle_summary_window_prims->runs[3].buffer = battle_area.buffer;
    }
}

/* Build the summary's change of the member's second stat, like 801dff50. */
void func_801E0184(u8 member) {
    s32 before;
    s32 after;
    s32 difference;
    s32 arrow;
    s32 j;
    s32 n;
    s32 digit;

    before = battle_work_area.savedMax[member][1];
    after = game_data.characters[battle_party_character_ids[member]].maxEp;
    difference = after - before;
    arrow = 0xE3;
    if (difference < 0) {
        arrow = 0xE5;
        difference = before - after;
    }
    if (difference != 0) {
        battle_summary_window_prims->runs[5].count = battle_build_glyph(arrow, battle_summary_window_prims->glyphs1900, 0xD8, 0x88);
        battle_split_decimal_digits(difference);
        n = 0;
        for (j = 0; j < 2; j++) {
            digit = battle_decimal_digits_plus_7[j];
            if (digit != 0xFF) {
                battle_summary_window_prims->runs[5].count += battle_build_glyph(digit, &battle_summary_window_prims->glyphs1900[battle_summary_window_prims->runs[5].count * 2], n * 8 + 0xE0, 0x88);
                n++;
            }
        }
        for (j = 0; j < battle_summary_window_prims->runs[5].count; j++) {
            SetShadeTex(&battle_summary_window_prims->glyphs1900[j * 2 + battle_area.buffer], 0);
            setRGB0(&battle_summary_window_prims->glyphs1900[j * 2 + battle_area.buffer], 0x80, 0x40, 0x40);
        }
        battle_summary_window_prims->runs[5].buffer = battle_area.buffer;
    }
}

/* Build the summary's member numbers and their changes. */
void func_801E03B8(u8 member) {
    func_801DFD58(member);
    func_801DFE6C(member);
    func_801DFF50(member);
    func_801E0184(member);
}

/* Build the member's seven gauge rows: for each, the value before and after
 * the battle out of the highest (801dfa38) as a bar and its change bar, the
 * value, and when it changed an arrow and the change shaded by direction.
 * The value's digits are entries 23-25 of battle_split_decimal_digits's digit buffer. */
void func_801E03FC(u8 member) {
    s32 max;
    s32 i;
    s32 j;
    s32 digit;
    s32 left;
    s32 n;
    s32 k;
    s32 top;
    s32 bottom;

    max = func_801DFA38(member);
    for (i = 0; i < 7; i++) {
        battle_summary_window_prims->rowACount[i] = 0;
        battle_summary_window_prims->rowBCount[i] = 0;
        func_801DF910(battle_work_area.savedStats[member][i], battle_work_area.resultStats[member][i], max);
        func_801DF710(battle_summary_window_prims->barA[i], 0);
        func_801DF710(battle_summary_window_prims->barB[i], D_801E44E0);
        top = i * 8 + 0x92;
        bottom = i * 8 + 0x98;
        (battle_summary_window_prims->barA[i] + battle_area.buffer)->x0 = 0x78;
        (battle_summary_window_prims->barA[i] + battle_area.buffer)->y0 = top;
        (battle_summary_window_prims->barA[i] + battle_area.buffer)->x1 = D_801E44D8 + 0x78;
        (battle_summary_window_prims->barA[i] + battle_area.buffer)->y1 = top;
        (battle_summary_window_prims->barA[i] + battle_area.buffer)->x2 = 0x78;
        (battle_summary_window_prims->barA[i] + battle_area.buffer)->y2 = bottom;
        (battle_summary_window_prims->barA[i] + battle_area.buffer)->x3 = D_801E44D8 + 0x78;
        (battle_summary_window_prims->barA[i] + battle_area.buffer)->y3 = bottom;
        if (D_801E44E0 == 2) {
            left = D_801E44D8 + 0x78;
        } else {
            left = D_801E44D8 + 0x78 - D_801E44DC;
        }
        (battle_summary_window_prims->barB[i] + battle_area.buffer)->x0 = left;
        (battle_summary_window_prims->barB[i] + battle_area.buffer)->y0 = top;
        (battle_summary_window_prims->barB[i] + battle_area.buffer)->x1 = left + D_801E44DC;
        (battle_summary_window_prims->barB[i] + battle_area.buffer)->y1 = top;
        (battle_summary_window_prims->barB[i] + battle_area.buffer)->x2 = left;
        (battle_summary_window_prims->barB[i] + battle_area.buffer)->y2 = bottom;
        (battle_summary_window_prims->barB[i] + battle_area.buffer)->x3 = left + D_801E44DC;
        (battle_summary_window_prims->barB[i] + battle_area.buffer)->y3 = bottom;
        battle_summary_window_prims->barBuffer[i] = battle_area.buffer;
        battle_split_decimal_digits(D_801E44CC);
        for (j = 0; j < 3; j++) {
            k = j + 23;
            digit = battle_decimal_digits_minus_17[k];
            if (digit != 0xFF) {
                battle_summary_window_prims->rowACount[i] += battle_build_glyph(digit, &battle_summary_window_prims->rowA[i][battle_summary_window_prims->rowACount[i] * 2], j * 8 + 0xB8, i * 8 + 0x90);
            }
        }
        battle_summary_window_prims->rowABuffer[i] = battle_area.buffer;
        if (D_801E44D4 != 0) {
            battle_summary_window_prims->rowBCount[i] = battle_build_glyph(D_801E44E4, battle_summary_window_prims->rowB[i], 0xD8, i * 8 + 0x90);
            battle_split_decimal_digits(D_801E44D4);
            n = 0;
            for (j = 0; j < 3; j++) {
                digit = battle_decimal_digits_plus_6[j];
                if (digit != 0xFF) {
                    battle_summary_window_prims->rowBCount[i] += battle_build_glyph(digit, &battle_summary_window_prims->rowB[i][battle_summary_window_prims->rowBCount[i] * 2], n * 8 + 0xE0, i * 8 + 0x90);
                    n++;
                }
            }
            func_801DF840(battle_summary_window_prims->rowB[i], D_801E44E0 - 2, battle_summary_window_prims->rowBCount[i], battle_area.buffer);
            battle_summary_window_prims->rowBBuffer[i] = battle_area.buffer;
        }
    }
}

/* Play effect id of the system effect bank. */
void func_801E09C0(u8 id) {
    sound_play_effect((sprite_script_sound_bank->bank << 16) | id);
}

/* Start the result fanfare's three effects once. */
void func_801E09F4(void) {
    if (D_801E44C0 == 0) {
        mode_result_fanfare_started = 1;
        func_801E09C0(0x5C);
        func_801E09C0(0x5D);
        func_801E09C0(0x5E);
        D_801E44C0 = 1;
    }
}

/* Start the fanfare and run battle frames until Cross is pressed. */
void func_801E0A4C(void) {
    func_801E09F4();
    battle_wait_frame();
    battle_ui->waitingCross = 1;
    while (battle_pressed_key != 4) {
        battle_wait_frame();
    }
    battle_ui->waitingCross = 0;
}

/* Show the skills the member learnt in the battle, one at a time: the
 * summary title, then each new counter skill and each new level skill's
 * name with its mark, waiting for Cross after each. */
void func_801E0ACC(u8 member) {
    u16 newCounter;
    u16 newLevel;
    s32 i;
    void *image;
    u8 width;
    RECT rect;

    newCounter = game_data.skills[battle_party_character_ids[member]].counterSkills & ~battle_known_skills_at_start[member].counterSkills;
    newLevel = game_data.skills[battle_party_character_ids[member]].levelSkills & ~battle_known_skills_at_start[member].levelSkills;
    if (newCounter == 0 && newLevel == 0) {
        return;
    }
    if (battle_ui->windows[1] == 0) {
        battle_window_open(1, 0x28, 0x78, 0xE8, 0x58, 0, 1);
    }
    battle_summary_window_prims->runs[0].count = battle_build_glyph(member + 0xFC, battle_summary_window_prims->title[0], 0x40, 0xA4);
    battle_summary_window_prims->runs[0].buffer = battle_area.buffer;
    battle_ui->waitingCross = 0;
    image = battle_heap_alloc_text_image(0x1B);
    battle_init_text_quad_pair(battle_summary_window_prims->glyphs34B0[0], 1, 2);
    battle_init_text_quad_pair(battle_summary_window_prims->glyphs34B0[1], 0, 3);
    for (i = 0; i < 16; i++) {
        if (battle_is_flag_in_mask(newCounter, i) != 0) {
            s32 *buffer = &battle_area.buffer;

            width = window_render_text_line(text_get_system_resource_entry(battle_party_character_ids[member], i), image, 0x1B, 0);
            rect.x = 0x3C0;
            rect.y = 0x1A;
            rect.w = 0x1E;
            rect.h = 0xD;
            battle_upload_image_and_wait(&rect, image);
            battle_quad_place_text_row(&battle_summary_window_prims->glyphs34B0[1][*buffer], 0x52, 0x9C, 0, 0x1A, width);
            SetShadeTex(&battle_summary_window_prims->glyphs34B0[1][*buffer], 0);
            setRGB0(&battle_summary_window_prims->glyphs34B0[1][battle_area.buffer], 0, 0x80, 0);
            battle_summary_window_prims->buffer34B0[1] = *buffer;
            battle_quad_place_text_row(&battle_summary_window_prims->glyphs34B0[0][*buffer], width + 0x5A, 0x9C, battle_skill_mark_icon_cell[2], battle_skill_mark_icon_cell[3], battle_skill_mark_icon_cell[0]);
            battle_summary_window_prims->buffer34B0[0] = *buffer;
            battle_ui->showSkill = 1;
            func_801E0A4C();
        }
    }
    for (i = 0; i < 16; i++) {
        if (battle_is_flag_in_mask(newLevel, i) != 0) {
            s32 *buffer = &battle_area.buffer;

            width = window_render_text_line(text_get_character_art_name(battle_party_character_ids[member] * 16 + i), image, 0x1B, 0);
            rect.x = 0x3C0;
            rect.y = 0x1A;
            rect.w = 0x1E;
            rect.h = 0xD;
            battle_upload_image_and_wait(&rect, image);
            battle_quad_place_text_row(&battle_summary_window_prims->glyphs34B0[1][*buffer], 0x52, 0x9C, 0, 0x1A, width);
            SetShadeTex(&battle_summary_window_prims->glyphs34B0[1][*buffer], 0);
            setRGB0(&battle_summary_window_prims->glyphs34B0[1][battle_area.buffer], 0x80, 0x80, 0);
            battle_summary_window_prims->buffer34B0[1] = *buffer;
            battle_quad_place_text_row(&battle_summary_window_prims->glyphs34B0[0][*buffer], width + 0x5A, 0x9C, battle_skill_mark_icon_cell[2], battle_skill_mark_icon_cell[3], battle_skill_mark_icon_cell[0]);
            battle_summary_window_prims->buffer34B0[0] = *buffer;
            battle_ui->showSkill = 1;
            func_801E0A4C();
        }
    }
    battle_ui->showSkill = 0;
}


/* Lay out the summary's seven-glyph label (2d30). */
void func_801E1044(void) {
    s32 i;

    for (i = 0; i < 7; i++) {
        battle_summary_window_prims->count2D30 += battle_build_glyph(battle_spoils_label_glyphs[i], battle_summary_window_prims->glyphs2D30[battle_summary_window_prims->count2D30], battle_spoils_label_x[i], battle_spoils_label_y[i]);
    }
    battle_summary_window_prims->buffer2D30 = battle_area.buffer;
}

/* Build the spoils window's numbers: the experience (six digits) and the
 * party gold (nine digits). */
void func_801E10F8(u32 experience) {
    s32 i;
    s32 n;
    s32 digit;

    battle_split_decimal_digits(experience);
    for (i = 0; i < 6; i++) {
        n = i + 27;
        digit = battle_camera_framed_range[n];
        if (digit != 0xFF) {
            battle_summary_window_prims->run2F60.count += battle_build_glyph(digit, battle_summary_window_prims->glyphs2F60[battle_summary_window_prims->run2F60.count], i * 8 + 0xD8, 0x50);
        }
    }
    battle_summary_window_prims->run2F60.buffer = battle_area.buffer;
    battle_split_decimal_digits(game_data.gold);
    for (i = 0; i < 9; i++) {
        n = i + 24;
        digit = battle_camera_framed_range[n];
        if (digit != 0xFF) {
            battle_summary_window_prims->run3140.count += battle_build_glyph(digit, battle_summary_window_prims->glyphs3140[battle_summary_window_prims->run3140.count], i * 8 + 0xC0, 0x60);
        }
    }
    battle_summary_window_prims->run3140.buffer = battle_area.buffer;
}

/* Build the spoils window's two icons. */
void func_801E126C(void) {
    s32 *buffer;

    battle_init_text_quad_pair(battle_summary_window_prims->glyphs3410[0], 0, 2);
    battle_init_text_quad_pair(battle_summary_window_prims->glyphs3410[1], 1, 2);
    buffer = &battle_area.buffer;
    battle_quad_place_text_row(&battle_summary_window_prims->glyphs3410[0][*buffer], 0x20, 0x20, battle_spoils_icon_cells[2], battle_spoils_icon_cells[3], battle_spoils_icon_cells[0]);
    battle_quad_place_text_row(&battle_summary_window_prims->glyphs3410[1][*buffer], 0xB8, 0x40, battle_spoils_icon_cells[6], battle_spoils_icon_cells[7], battle_spoils_icon_cells[4]);
    battle_summary_window_prims->buffer3410 = *buffer;
}

/* Add count of item id to an inventory list of size entries (ids and
 * counts): stack onto the item (at most 99) or take the first free entry;
 * a full list drops the item. */
void func_801E1370(u8 id, u8 count, u8 *ids, u8 *counts, u8 size) {
    s32 i;

    for (i = 0; i < size; i++) {
        if (ids[i] == id) {
            if (counts[i] + count >= 100) {
                counts[i] = 99;
            } else {
                counts[i] = count + counts[i];
            }
            break;
        }
    }
    if (i == size) {
        for (i = 0; i < size; i++) {
            if (ids[i] == 0) {
                ids[i] = id;
                counts[i] = count;
                break;
            }
        }
    }
}

/* Add eight drops (ids, counts and inventory list categories) to the
 * inventory. */
void func_801E1444(u8 *ids, u8 *counts, u8 *categories) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (ids[i] != 0) {
            switch (categories[i]) {
            case 0:
                func_801E1370(ids[i], counts[i], game_data.weaponIds, game_data.weaponCounts, 100);
                break;
            case 1:
                func_801E1370(ids[i], counts[i], game_data.accessoryIds, game_data.accessoryCounts, 200);
                break;
            case 2:
                func_801E1370(ids[i], counts[i], game_data.itemIds, game_data.itemCounts, 150);
                break;
            case 3:
                func_801E1370(ids[i], counts[i], game_data.gearPartIds, game_data.gearPartCounts, 100);
                break;
            case 4:
                func_801E1370(ids[i], counts[i], game_data.gearAccessoryIds, game_data.gearAccessoryCounts, 150);
                break;
            }
        }
    }
}

/* Collect the rolled drops into eight distinct (category, id) entries
 * with their counts. */
void func_801E1590(u8 *ids, u8 *counts, u8 *categories) {
    s32 i;
    s32 j;
    s32 k;

    for (i = 0; i < 8; i++) {
        ids[i] = 0;
        counts[i] = 0;
    }
    k = 0;
    for (i = 0; i < 8; i++) {
        if (battle_work_area.dropIds[i] != 0) {
            for (j = 0; j < 8; j++) {
                if (battle_work_area.dropCategories[i] == categories[j] && battle_work_area.dropIds[i] == ids[j]) {
                    counts[j]++;
                    break;
                }
            }
            if (j == 8) {
                categories[k] = battle_work_area.dropCategories[i];
                ids[k] = battle_work_area.dropIds[i];
                counts[k]++;
                k++;
            }
        }
    }
}

/* Build the spoils window's item list: collect the drops, render each
 * item's name into VRAM with its count, then add the drops to the
 * inventory. */
void func_801E1690(void) {
    u8 ids[8];
    u8 categories[8];
    u8 counts[8];
    RECT rect;
    void *text[8];
    void **names; /* the text buffers as the name loop fills them */
    s32 i;
    s32 count;
    s32 width;

    func_801E1590(ids, counts, categories);
    names = text;
    for (i = 0, count = 0; i < 8; i++) {
        if (ids[i] != 0) {
            battle_init_text_quad_pair(battle_summary_window_prims->listA[count], 0, 1);
            battle_init_text_quad_pair(battle_summary_window_prims->listB[count], 0, 2);
            names[count] = battle_heap_alloc_text_image(0x1B);
            switch (categories[i]) {
            case 0:
                width = window_render_text_line(text_get_weapon_name(ids[i]), names[count], 0x1B, 0);
                break;
            case 1:
                width = window_render_text_line(text_get_accessory_name(ids[i]), names[count], 0x1B, 0);
                break;
            case 2:
                width = window_render_text_line(text_get_item_name(ids[i]), names[count], 0x1B, 0);
                break;
            case 3:
                width = window_render_text_line(text_get_gear_part_name(ids[i]), names[count], 0x1B, 0);
                break;
            case 4:
                width = window_render_text_line(text_get_gear_accessory_name(ids[i]), names[count], 0x1B, 0);
                break;
            }
            rect.x = 0x380;
            rect.y = count * 13 + 0x100;
            rect.w = 0x1E;
            rect.h = 0xD;
            battle_upload_image_and_wait(&rect, names[count]);
            battle_quad_place_text_row(&battle_summary_window_prims->listA[count][battle_area.buffer], 0x2C, count * 16 + 0x30, 0, count * 13, width);
            battle_quad_place_text_row(&battle_summary_window_prims->listB[count][battle_area.buffer], 0x94, count * 16 + 0x30, counts[i] * 8 + 0x78, 0, 8);
            count++;
        }
    }
    func_801E1444(ids, counts, categories);
    for (i = 0; i < count; i++) {
        heap_free(text[i]);
    }
    battle_summary_window_prims->listCount = count;
    battle_summary_window_prims->listBuffer = battle_area.buffer;
}

/* Show the member cards over six frames, then wait for Cross. */
void func_801E196C(void) {
    u32 step;
    u8 building;

    step = 0;
    building = 1;
    do {
        battle_wait_frame();
        switch (step) {
        case 0:
            func_801DE5C4();
            battle_ui->showCards = 1;
            battle_turn_state->eventsDone = 0;
            break;
        case 1:
            func_801DE69C();
            break;
        case 2:
            func_801DEA18();
            break;
        case 3:
            func_801DEDC0(0);
            break;
        case 4:
            func_801DF270();
            break;
        case 5:
            func_801DF4C0();
            building = 0;
            break;
        }
        step++;
    } while (building);
    battle_ui->waitingCross = 1;
    while (battle_pressed_key != 4) {
        battle_wait_frame();
    }
    battle_ui->waitingCross = 0;
}

/* Wait for Cross on the first member card (repeating the prompt sound),
 * then take each member's two values from the game data and rebuild the
 * summary rows. */
void func_801E1AA4(void) {
    s32 i;

    battle_wait_frame();
    battle_ui->waitingCross = 1;
    battle_member_cards[0]->counting = 1;
    while (battle_member_cards[0]->counting != 0) {
        if (battle_pressed_key == 4) {
            break;
        }
        func_801E09C0(0x5B);
        battle_wait_frame();
    }
    battle_ui->waitingCross = 0;
    battle_member_cards[0]->counting = 0;
    for (i = 0; i < 3; i++) {
        battle_work_area.expTotals[i][0] = game_data.characters[battle_party_character_ids[i]].expTotalA;
        battle_work_area.expTotals[i][1] = game_data.characters[battle_party_character_ids[i]].expTotalB;
        battle_work_area.toCount[i][0] = 0;
        battle_work_area.toCount[i][1] = 0;
    }
    func_801DF270();
    func_801DF4C0();
}

/* After Cross, show each present member's summary window when a stat
 * changed (waiting for Cross), then its skill results (801e0acc). */
void func_801E1C10(void) {
    s32 i;
    u8 shown;
    u8 member;

    func_801DEDC0(1);
    battle_wait_frame();
    battle_ui->waitingCross = 1;
    while (battle_pressed_key != 4) {
        battle_wait_frame();
    }
    battle_ui->waitingCross = 0;
    battle_wait_frame();
    battle_window_open(1, 0x28, 0x78, 0xE8, 0x58, 0, 0);
    battle_ui->windows[1] = 0;
    for (i = 0; i < 3; i++) {
        shown = 0;
        if (battle_area.slots[i].field2 != 0x7F) {
            if (battle_work_area.levelGains[i][0] != 0) {
                member = i;
                battle_ui->windows[1] = 1;
                func_801DFAA8(member);
                func_801E03B8(member);
                func_801E03FC(member);
                battle_ui->showSummary = 1;
                func_801E09F4();
                battle_ui->waitingCross = 0;
                shown = 1;
                battle_wait_frame();
            }
            battle_ui->waitingCross = 1;
            while (battle_pressed_key != 4 && shown) {
                battle_wait_frame();
            }
            battle_ui->showSummary = 0;
            battle_wait_frame();
            func_801E0ACC(i);
            battle_ui->waitingCross = 0;
            battle_ui->windows[1] = 0;
        }
    }
}

/* Show the spoils window (experience, gold, items) until Cross. */
void func_801E1E10(u32 experience) {
    battle_ui->showCards = 0;
    battle_ui->showSummary = 0;
    battle_ui->showSkill = 0;
    battle_wait_frame();
    battle_window_open(0, 0x18, 0x18, 0x90, 0xA0, 0, 1);
    battle_window_open(2, 0xB0, 0x38, 0x70, 0x38, 0, 1);
    battle_wait_frame();
    func_801E1044();
    func_801E10F8(experience);
    func_801E126C();
    func_801E1690();
    battle_ui->showSpoils = 1;
    sound_play_effect_on_last_channels((sprite_script_sound_bank->bank << 16) | 0x5B);
    battle_ui->waitingCross = 1;
    while (battle_pressed_key != 4) {
        battle_wait_frame();
    }
    battle_ui->waitingCross = 0;
    battle_ui->showSpoils = 0;
    battle_ui->windows[0] = 0;
    battle_ui->windows[1] = 0;
    battle_ui->windows[2] = 0;
    battle_wait_frame();
    battle_window_close(0);
    battle_window_close(1);
    battle_window_close(2);
}

/* The battle results: allocate the member cards and the summary, show the
 * cards, the summaries and the spoils, then release them. */
void func_801E1FB8(u32 experience) {
    u8 saved;
    s32 i;

    saved = battle_area.outcome;
    battle_ui->waitingCross = 0;
    for (i = 0; i < 3; i++) {
        battle_member_cards[i] = battle_heap_alloc(sizeof(MemberCard), 0);
        bzero((u8 *)battle_member_cards[i], sizeof(MemberCard));
    }
    battle_summary_window_prims = battle_heap_alloc(sizeof(ResultSummary), 0);
    bzero((u8 *)battle_summary_window_prims, sizeof(ResultSummary));
    battle_member_cards[0]->secondValue = (game_data.flags >> 15) ^ 1;
    battle_wait_frame();
    battle_area.outcome = 0;
    func_801E196C();
    func_801E1AA4();
    func_801E1C10();
    func_801E1E10(experience);
    battle_ui->showCards = 0;
    battle_ui->showSummary = 0;
    battle_ui->showSkill = 0;
    battle_wait_frame();
    for (i = 0; i < 3; i++) {
        heap_free(battle_member_cards[i]);
    }
    heap_free(battle_summary_window_prims);
    battle_area.outcome = saved;
    sound_stop_all_effects();
}

/* Hide the battle windows and reload the results resources: archive file
 * 2 of directory 0x10 (its items 1-4: text, a table, the glyph sprites and
 * the portraits). */
void func_801E211C(void) {
    u8 unused[0x60]; /* unused in the original; reserves 96 bytes */
    ResultArchive *archive;
    void *data;

    battle_frame_mode = 0;
    battle_ui->showCards = 0;
    battle_ui->showSummary = 0;
    battle_ui->showSkill = 0;
    battle_ui->showSpoils = 0;
    battle_ui->barShown[0] = battle_ui->barShown[1] = battle_ui->barShown[2] = 0;
    battle_wait_frame();
    battle_wait_frame();
    heap_free(battle_glyph_table);
    cd_select_directory(0x10, 2);
    archive = battle_heap_alloc(cd_get_aligned_file_size(2), 1);
    cd_read_file(2, archive, 0, 0x80);
    battle_cd_wait_for_reads();
    text_relocate_offset_table(archive);
    battle_work_growth_file[0] = text_unpack_lzss_alloc(archive->items[0], 0);
    data = text_unpack_lzss_alloc(archive->items[2], 0);
    model_load_tim_list(data);
    heap_free(data);
    battle_glyph_table = text_unpack_lzss_alloc(archive->items[1], 0);
    data = text_unpack_lzss_alloc(archive->items[3], 0);
    battle_upload_party_portraits(data, 0xFC);
    heap_free(data);
    heap_free(archive);
    battle_upload_command_name_images();
}

/* Total the experience and gold of the defeated enemies, add the gold (up
 * to 9999999), clear empty party slots, grant the rewards and run the
 * result screens. */
void func_801E2280(void) {
    s32 i;
    u32 gold;
    u32 *partyGold;

    gold = 0;
    battle_work_area.experience = 0;
    battle_work_area.defeated = 0;
    if (battle_exit_requested == 0) {
        for (i = 0; i < 8; i++) {
            if (battle_turn_queue.present[i + 3] != 0 && battle_area.slots[i + 3].hidden == 0
                && (battle_work_area.records[i + 3].pilot.status7C & 0x8000) && battle_enemy_no_reward_flags[i][0] == 0) {
                battle_work_area.experience += battle_work_area.records[i + 3].field14C;
                gold += battle_work_area.records[i + 3].field156;
                battle_work_area.defeated |= battle_get_slot_bit(i);
            }
        }
        partyGold = &game_data.gold;
        *partyGold += gold;
        if (*partyGold > 9999999) {
            *partyGold = 9999999;
        }
        battle_highlight_slots(0);
    }
    for (i = 0; i < 3; i++) {
        if (battle_party_character_ids[i] == 0x7F) {
            battle_party_character_ids[i] = 0xFF;
        }
    }
    if (battle_uses_fixed_party != 0) {
        battle_party_character_ids[1] = battle_party_character_ids[2] = 0xFF;
        battle_area.slots[1].field2 = battle_area.slots[2].field2 = 0x7F;
    }
    func_801E2794();
    if (battle_skip_result_screens == 0 && !(formation_active.flags & 8)) {
        func_801E1FB8(gold);
    }
}

/* Write the battle item counts back to inventory list 2. */
void func_801E24B0(void) {
    s32 i;
    s32 j;
    u8 *item;

    for (i = 0; i < 48; i++) {
        item = &battle_item_inventory_ids[i];
        if (*item != 0) {
            for (j = 0; j < 150; j++) {
                if (*item == game_data.itemIds[j]) {
                    game_data.itemCounts[j] = battle_item_counts[i];
                }
            }
        }
    }
}

/* Leave the battle: reload the resources, pick the next mode, write the
 * items back, grant the rewards (unless the battle was escaped or they are
 * skipped), release the battle's blocks and windows and reset the sound. */
void func_801E252C(void) {
    s32 i;
    u8 *outcome;

    func_801E211C();
    if (battle_uses_event_script != 0) {
        heap_free((void *)battle_heap_mark_for_event_script);
        heap_free((void *)battle_heap_reserve_for_event_script);
    }
    if (mode_battle_standalone == 0) {
        if (battle_continue_to_movie_mode != 0) {
            mode_load_overlay_block(6);
        } else if (mode_pending_battle_formation != 0) {
            mode_load_overlay_block(2);
        } else if ((game_data.map & 0x7FF) >= 0x400) {
            mode_load_overlay_block(3);
        } else {
            mode_preload_field_files();
            battle_cd_wait_for_reads();
            mode_load_overlay_block(1);
        }
    }
    battle_cd_wait_for_reads();
    func_801E24B0();
    outcome = &battle_area.outcome;
    if (!(*outcome & 0xC0) && *outcome != 0x21 && battle_exit_requested == 0 && mode_result_code != 3) {
        battle_load_wave_bank_5();
        func_801E2280();
    }
    for (i = 0; i < 8; i += 2) {
        heap_free(battle_message_pixel_blocks[i].data);
    }
    for (i = 0; i < 10; i++) {
        heap_free(battle_digit_text_images[i].pixels);
    }
    heap_free(battle_item_name_table);
    heap_free(battle_direction_arrows);
    battle_window_close(5);
    battle_window_close(4);
    heap_free(battle_message_table);
    heap_free(battle_graphics);
    heap_free(battle_ui);
    heap_free(battle_turn_state);
    heap_free(battle_work_growth_file[0]);
    heap_free(battle_glyph_table);
    heap_free_tag(2);
    if (mode_battle_standalone != 0) {
        sound_stop_seq((SoundSeq *)battle_music_seq);
        sound_release_seq((SoundSeq *)battle_music_seq);
    }
    battle_leave();
}

/* Grant the battle rewards unless the whole party is knocked out; then, when
 * character 3 pilots gear 0x12, set his skill flags 1A to 0x4000, with 0x8000
 * too when gear 12 has a part kind 5 amount. */
void func_801E2794(void) {
    u8 slot;
    u8 knockedOut;
    u8 extra;

    knockedOut = 0;
    for (slot = 0; slot < 3; slot++) {
        if (battle_work_area.records[slot].pilot.status7C & 0x8000) {
            knockedOut++;
        }
    }
    if (knockedOut != 3) {
        D_801E44E8 = D_801E44C8->growth;
        func_801E2ACC();
        func_801E3A18();
        func_801E403C();
        func_801E41B4();
        func_801E2888();
        func_801E42C4();
        if (game_data.characters[3].gearId == 0x12) {
            extra = game_data.gears[12].field4F;
            game_data.skills[3].flags1A = 0x4000;
            if (extra != 0) {
                game_data.skills[3].flags1A = 0xC000;
            }
        }
    }
}

/* Write each party member's HP, EP, counters and gear HP and fuel back to
 * the game data, clamped to their maximums (HP 1 when knocked out, gear HP a
 * tenth of the maximum when destroyed). */
void func_801E2888(void) {
    s32 unused[2]; /* unused in the original; reserves 8 bytes */
    u8 slot;
    u8 k;
    Combatant *record;
    CharacterRecord *character;
    GearRecord *gear;
    GearRecord *block;

    for (slot = 0; slot < 3; slot++) {
        if (battle_party_character_ids[slot] == 0xFF) {
            continue;
        }
        record = &D_801E44C8->records[slot];
        character = &D_801E44C4->characters[record->pilot.characterId];
        gear = &D_801E44C4->gears[record->pilot.gearId];
        block = &record->gear;
        if (record->pilot.characterId == 7 && (battle_work_area.records[slot].flags15A & 0x80)) {
            record->pilot.hp = (block->hp + 1) / 50;
            if (record->pilot.hp == 0) {
                record->pilot.hp = 1;
            }
        }
        character->hp = record->pilot.hp;
        character->ep = record->pilot.ep;
        if (character->hp > character->maxHp) {
            character->hp = character->maxHp;
        }
        if (character->ep > character->maxEp) {
            character->ep = character->maxEp;
        }
        for (k = 0; k < 7; k++) {
            character->useCounts[k] = record->pilot.useCounts[k];
        }
        character->field3A = record->pilot.field3A;
        if (record->pilot.status7C & 0xC000) {
            character->hp = 1;
        }
        switch (record->pilot.gearId) {
        case 0 ... 6:
        case 8 ... 16:
            gear->hp = block->hp;
            gear->fuel = block->fuel;
            if (gear->hp > gear->maxHp) {
                gear->hp = gear->maxHp;
            }
            if (gear->fuel > gear->maxFuel) {
                gear->fuel = gear->maxFuel;
            }
            if (block->status7C & 0x8000) {
                gear->hp = gear->maxHp / 10;
            }
            break;
        }
    }
}

/* Distribute the experience won: party members that stand share it (less
 * the penalty), every other character gets a reserve share of a third;
 * then record each slot's level gains and result stats. */
void func_801E2ACC(void) {
    s16 slots[11];
    u16 i;
    u16 absent;
    u32 experience;

    for (i = 0; i < 11; i++) {
        slots[i] = 100;
    }
    absent = 0;
    for (i = 0; i < 3; i++) {
        if (battle_party_character_ids[i] == 0xFF) {
            absent++;
            continue;
        }
        D_801E44EC = &D_801E44C8->records[i].pilot;
        if (D_801E44EC->status7C & 0xC000) {
            absent++;
            slots[D_801E44EC->characterId] = 0xFF;
        } else {
            slots[D_801E44EC->characterId] = i;
        }
        D_801E44F8[i][0] = D_801E44EC->level;
        D_801E44F8[i][1] = D_801E44EC->level2;
    }
    experience = D_801E44C8->experience;
    if (D_801E44C8->penalty != 0) {
        experience -= (experience / 4) * D_801E44C8->penalty;
    }
    for (i = 0; i < 11; i++) {
        if (slots[i] == 0xFF) {
            continue;
        }
        D_801E44EC = &D_801E44C4->characters[i];
        if (slots[i] < 3) {
            func_801E2EB0(experience / (3 - absent), slots[i], 0);
        } else {
            func_801E2EB0(experience / 3, 0xFF, 1);
        }
        func_801E308C();
    }
    for (i = 0; i < 3; i++) {
        if (battle_party_character_ids[i] == 0xFF) {
            continue;
        }
        D_801E44EC = &D_801E44C4->characters[battle_party_character_ids[i]];
        D_801E44C8->levelGains[i][0] = D_801E44EC->level - D_801E44F8[i][0];
        D_801E44C8->levelGains[i][1] = D_801E44EC->level2 - D_801E44F8[i][1];
        if (D_801E44EC->characterId == 4) {
            D_801E44C8->resultStats[i][0] = D_801E44EC->entries[0].value4 + D_801E44EC->entries[3].value4;
        } else {
            D_801E44C8->resultStats[i][0] = D_801E44EC->attack + D_801E44EC->entries[0].value4;
        }
        D_801E44C8->resultStats[i][1] = D_801E44EC->field5E;
        D_801E44C8->resultStats[i][2] = D_801E44EC->defense + D_801E44EC->bodyDefense;
        D_801E44C8->resultStats[i][3] = D_801E44EC->field5F;
        D_801E44C8->resultStats[i][4] = D_801E44EC->accuracy;
        D_801E44C8->resultStats[i][5] = D_801E44EC->etherDefense;
        D_801E44C8->resultStats[i][6] = D_801E44EC->speed;
    }
}

/* Split a slot's experience into the level A and B pools by the record's
 * weights (three quarters each for a reserve member, all of it with option
 * 0x8000), at least 1, raised by half by flags 0x2000/0x1000; kept per slot. */
void func_801E2EB0(u32 experience, s16 slot, s16 reserve) {
    s16 weightA;
    s16 weightB;

    if (reserve == 1) {
        experience = experience * 3 / 4;
        D_801E44F0 = experience;
        D_801E44F4 = experience;
        return;
    }
    weightA = D_801E44C8->records[slot].expWeightA;
    weightB = D_801E44C8->records[slot].expWeightB;
    if (weightA < 2) {
        weightA = 1;
    }
    if (weightB < 2) {
        weightB = 1;
    }
    D_801E44F0 = experience * weightA / (weightA + weightB);
    D_801E44F4 = experience * weightB / (weightA + weightB);
    if (D_801E44C4->flags & 0x8000) {
        D_801E44F0 = experience;
        D_801E44F4 = experience;
    }
    if (D_801E44F0 == 0) {
        D_801E44F0 = 1;
    }
    if (D_801E44F4 == 0) {
        D_801E44F4 = 1;
    }
    if (D_801E44EC->flags32 & 0x2000) {
        D_801E44F0 += D_801E44F0 >> 1;
        D_801E44F4 += D_801E44F4 >> 1;
    }
    if (D_801E44EC->flags32 & 0x1000) {
        D_801E44F4 += D_801E44F4 >> 1;
    }
    D_801E44C8->toCount[slot][0] = D_801E44F0;
    D_801E44C8->toCount[slot][1] = D_801E44F4;
}

/* Add the experience pools to the current record's totals and gain levels
 * A and B while the pools reach the next level (none past 99 with option
 * 0x8000, and nothing more at 99). */
void func_801E308C(void) {
    s32 rest;

    D_801E44EC->expTotalA += D_801E44F0;
    D_801E44EC->expTotalB += D_801E44F4;
    if (D_801E44EC->level == 99 && (D_801E44C4->flags & 0x8000)) {
        D_801E44F0 = 0;
    }
    if (D_801E44EC->level2 == 99 && (D_801E44C4->flags & 0x8000)) {
        D_801E44F4 = 0;
    }
    if (D_801E44EC->level == 99) {
        D_801E44F0 = 0;
    }
    if (D_801E44EC->level2 == 99) {
        D_801E44F4 = 0;
    }
    rest = D_801E44EC->expNextA - D_801E44F0;
    if (rest > 0) {
        D_801E44EC->expNextA = rest;
    } else {
        do {
            if (++D_801E44EC->level >= 100 && (D_801E44C4->flags & 0x8000)) {
                D_801E44EC->level--;
            }
            D_801E44EC->expNextA = D_801E44E8->experience[D_801E44EC->level - 1];
            func_801E335C();
            rest += D_801E44EC->expNextA;
        } while (rest <= 0);
    }
    D_801E44EC->expNextA = rest;
    rest = D_801E44EC->expNextB - D_801E44F4;
    if (rest > 0) {
        D_801E44EC->expNextB = rest;
    } else {
        do {
            if (++D_801E44EC->level2 >= 100 && (D_801E44C4->flags & 0x8000)) {
                D_801E44EC->level2--;
            }
            D_801E44EC->expNextB = D_801E44E8->experience[D_801E44EC->level2 - 1];
            func_801E3500();
            rest += D_801E44EC->expNextB;
        } while (rest <= 0);
    }
    D_801E44EC->expNextB = rest;
}

/* Level A growth of the current record: max HP, then stats 58, 59, 5e
 * and 5f toward the growth data's targets for its level range. */
void func_801E335C(void) {
    u8 high;
    u8 cap;
    u8 level;

    high = 0;
    cap = 100;
    if (D_801E44EC->level >= 100) {
        high = 1;
        cap = 200;
    }
    level = D_801E44EC->level;
    D_801E44EC->maxHp = func_801E3700(D_801E44EC->maxHp, D_801E44EC->level);
    D_801E44EC->attack = func_801E3610(D_801E44EC->attack,
        D_801E44E8->characters[D_801E44EC->characterId].statTargets[0][high], cap, level);
    D_801E44EC->defense = func_801E3610(D_801E44EC->defense,
        D_801E44E8->characters[D_801E44EC->characterId].statTargets[1][high], cap, level);
    D_801E44EC->field5E = func_801E3610(D_801E44EC->field5E,
        D_801E44E8->characters[D_801E44EC->characterId].statTargets[2][high], cap, level);
    D_801E44EC->field5F = func_801E3610(D_801E44EC->field5F,
        D_801E44E8->characters[D_801E44EC->characterId].statTargets[3][high], cap, level);
}

/* Level B growth of the current record: max EP, then stats 5b and 5c
 * toward the growth data's targets for its level range. */
void func_801E3500(void) {
    u8 high;
    u8 cap;
    u8 level;

    high = 0;
    cap = 100;
    if (D_801E44EC->level2 >= 100) {
        high = 1;
        cap = 200;
    }
    level = D_801E44EC->level2;
    D_801E44EC->maxEp = func_801E38CC(D_801E44EC->maxEp, D_801E44EC->level2);
    D_801E44EC->accuracy = func_801E3610(D_801E44EC->accuracy,
        D_801E44E8->characters[D_801E44EC->characterId].statTargets[4][high], cap, level);
    D_801E44EC->etherDefense = func_801E3610(D_801E44EC->etherDefense,
        D_801E44E8->characters[D_801E44EC->characterId].statTargets[5][high], cap, level);
}

/* Grow a stat by 0 or 1: the chance is the share of the distance to the
 * target left over the levels to the cap. Capped at 200. */
/* Grow a stat by 0 or 1: the chance is the share of the distance to the
 * target left over the levels to the cap. Capped at 200. */
u8 func_801E3610(u8 stat, u8 target, u8 cap, u8 level) {
    s32 random;
    s32 share;
    u8 grow;

    random = rand();
    share = (target - stat) * 100 / (cap - level) - 50;
    grow = (s16)(share + random % 100) > 49;
    stat += grow;
    if (stat > 200) {
        stat = 200;
    }
    return stat;
}

/* Grow max HP by a random share of the distance to the growth data's
 * target for the level range, at least 2. Capped at 999. */
u16 func_801E3700(u16 maxHp, u8 level) {
    s16 gain;
    u16 hp;
    u16 result;
    s32 top = 99; /* the first range's top level */

    hp = maxHp;
    if (level < 100) {
        gain = rand() % 100 * (D_801E44E8->characters[D_801E44EC->characterId].maxHpTargets[0] - (level - top) - hp)
            / ((100 - level) * 100) + 2;
        if (gain < 0) {
            result = hp;
        } else {
            result = gain + maxHp;
        }
    } else {
        gain = rand() % 100 * ((D_801E44E8->characters[D_801E44EC->characterId].maxHpTargets[1] - hp)
            / ((201 - level) * 100)) * 2 + 2;
        if (gain < 0) {
            result = hp;
        } else {
            result = gain + maxHp;
        }
    }
    if ((s16)result >= 1000) {
        result = 999;
    }
    return result;
}

/* Grow max EP by 0 or 1 toward the growth data's target for the level
 * range (as func_801E3610). Capped at 99. */
u8 func_801E38CC(u8 maxEp, u8 level) {
    u8 target;
    u8 cap;
    s32 random;
    s32 share;
    u8 grow;

    if (level < 100) {
        target = D_801E44E8->characters[D_801E44EC->characterId].maxEpTargets[0];
        cap = 99;
    } else {
        target = D_801E44E8->characters[D_801E44EC->characterId].maxEpTargets[1];
        cap = 200;
    }
    random = rand();
    share = (target - maxEp) * 100 / (cap - level) - 50;
    grow = (s16)(share + random % 100) > 49;
    maxEp += grow;
    if (maxEp >= 100) {
        maxEp = 99;
    }
    return maxEp;
}

/* Learn skills for each party slot that is not knocked out: a counter skill
 * (not characters 7 and 8), a level skill (not 10), the unlocks, and the
 * special cases of characters 8 and 7. */
void func_801E3A18(void) {
    u8 slot;
    u8 learnt;

    for (slot = 0; slot < 3; slot++) {
        D_801E44EC = &D_801E44C8->records[slot].pilot;
        if (D_801E44EC->status7C & 0x8000) {
            continue;
        }
        switch (D_801E44EC->characterId) {
        case 7:
        case 8:
            break;
        default:
            learnt = func_801E3BE0(D_801E44EC->characterId);
            if (learnt != 0) {
                D_801E44C8->learntCounter[slot] = learnt;
            }
            break;
        }
        if (D_801E44EC->characterId != 10) {
            learnt = func_801E3D54(D_801E44EC->characterId);
            if (learnt != 0) {
                D_801E44C8->learntLevel[slot] = learnt;
            }
        }
        switch (D_801E44EC->characterId) {
        case 7:
        case 8:
        case 10:
            break;
        default:
            func_801E3E14(D_801E44EC->characterId);
            break;
        }
        switch (D_801E44EC->characterId) {
        case 8:
        case 9:
        case 10:
            break;
        default:
            func_801E3F28(D_801E44EC->characterId);
            break;
        }
        if (D_801E44EC->characterId == 8) {
            func_801E3EA4();
        }
        if (D_801E44EC->characterId == 7) {
            func_801E3FB0();
        }
    }
}

/* Learn the first unknown counter skill (of 7, or 13 with option 0x4000)
 * whose level is reached and whose seven counter requirements the current
 * record meets; stop at the first whose level is not reached. Returns its
 * index, or 0. */
u8 func_801E3BE0(u8 id) {
    u8 count;
    u8 learnt;
    u8 j;
    u8 k;
    s32 bit;

    count = 7;
    if (game_data.flags & 0x4000) {
        count = 13;
    }
    learnt = 0xFF;
    for (j = 0; j < count; j++) {
        bit = 0x8000;
        if (game_data.skills[id].counterSkills & (bit >> j)) {
            continue;
        }
        if (D_801E44E8->characters[id].counterLevels[j] > game_data.characters[id].level) {
            break;
        }
        for (k = 0; k < 7; k++) {
            if (D_801E44E8->characters[id].requirements[j][k] > D_801E44EC->useCounts[k]) {
                break;
            }
        }
        if (k == 7) {
            learnt = j;
            break;
        }
    }
    if (learnt == 0xFF) {
        return 0;
    }
    game_data.skills[id].counterSkills |= 0x8000 >> learnt;
    return learnt;
}

/* Learn the first of the character's twelve level skills whose level is
 * reached and which is not yet known. Returns its number (1-12), or 0. */
u8 func_801E3D54(u8 id) {
    u32 bit;
    u8 k;
    u8 level;
    u32 known;

    bit = 0x8000;
    for (k = 0; k < 12; k++, bit >>= 1) {
        level = D_801E44E8->characters[id].levelSkills[k];
        if (level == 0xFF) {
            return 0;
        }
        if (game_data.characters[id].level >= level) {
            known = D_801E44C4->skills[id].levelSkills;
            if (!(bit & known)) {
                D_801E44C4->skills[id].levelSkills = bit | known;
                return k + 1;
            }
        }
    }
    return 0;
}

/* For each of the character's nine unlock entries whose counter skill is
 * known, set the matching unlock bit (from bit 3). */
void func_801E3E14(u8 id) {
    u8 k;
    u8 entry;

    for (k = 0; k < 9; k++) {
        entry = D_801E44E8->characters[id].unlocksA[k];
        if (entry == 0xFF) {
            return;
        }
        if (D_801E44C4->skills[id].counterSkills & (0x8000 >> (entry - 1))) {
            D_801E44C4->skills[id].unlocksA |= 0x8000 >> (k + 3);
        }
    }
}

/* Character 8's nine level entries: learn each one its level reaches. */
void func_801E3EA4(void) {
    u8 k;
    u8 level;
    u32 known;
    s32 bit;

    for (k = 0; k < 9; k++) {
        level = D_801E44E8->characters[8].unlocksA[k];
        if (level == 0xFF) {
            return;
        }
        if (game_data.characters[8].level >= level) {
            known = D_801E44C4->skills[8].unlocksA;
            bit = 0x1000 >> k;
            if (!(known & bit)) {
                D_801E44C4->skills[8].unlocksA = bit | known;
            }
        }
    }
}

/* For each of the character's thirteen second unlock entries whose level
 * skill is known, set the matching unlock bit. */
void func_801E3F28(u8 id) {
    u8 k;
    u8 entry;

    for (k = 0; k < 13; k++) {
        entry = D_801E44E8->characters[id].unlocksB[k];
        if (entry == 0) {
            return;
        }
        if (D_801E44C4->skills[id].levelSkills & (0x8000 >> (entry - 1))) {
            D_801E44C4->skills[id].unlocksB |= 0x8000 >> k;
        }
    }
}

/* Character 7's derived values from the current record's max HP and
 * attack. */
void func_801E3FB0(void) {
    GameData *game = D_801E44C4;

    game->gears[7].maxHp = D_801E44EC->maxHp * 200;
    game->gears[7].attack = D_801E44EC->attack / 5 + 1;
    game->gears[7].bodyDefense = D_801E44EC->maxHp * 10;
    game->gears[7].armor = D_801E44EC->maxHp * 10;
}

/* Advance each character's tier: 3, 4 and 5 at the growth data's tier
 * levels, 6 to 7 at level 50 with option 0x4000. */
void func_801E403C(void) {
    u8 id;
    CharacterRecord *character;
    CharacterSkills *skills;

    for (id = 0; id < 11; id++) {
        character = &D_801E44C4->characters[id];
        skills = &D_801E44C4->skills[id];
        if (skills->tier == 7) {
            continue;
        }
        switch (skills->tier) {
        case 3:
            if (character->level >= D_801E44E8->characters[id].tierLevels[0]) {
                skills->tier = 4;
            }
            break;
        case 4:
            if (character->level >= D_801E44E8->characters[id].tierLevels[1]) {
                skills->tier = 5;
            }
            break;
        case 5:
            if (character->level >= D_801E44E8->characters[id].tierLevels[2]) {
                skills->tier = 6;
            }
            break;
        case 6:
            if (character->level >= 50 && (game_data.flags & 0x4000)) {
                skills->tier = 7;
            }
            break;
        }
    }
}

/* Levels 50, 60 and 70 set unlock bits 8, 4 and 2 of each party member. */
void func_801E41B4(void) {
    u8 slot;
    u8 id;

    for (slot = 0; slot < 3; slot++) {
        id = battle_work_area.records[slot].pilot.characterId;
        if (game_data.characters[id].level >= 50) {
            game_data.skills[id].unlocksA |= 8;
        }
        if (game_data.characters[id].level >= 60) {
            game_data.skills[id].unlocksA |= 4;
        }
        if (game_data.characters[id].level >= 70) {
            game_data.skills[id].unlocksA |= 2;
        }
    }
}

/* Roll one drop per defeated enemy: the first at its chance (always when a
 * party member has flag 0x800), else the second at its chance. */
void func_801E42C4(void) {
    u8 i;
    u8 forced;
    s32 low_bit;
    s32 bit;
    Combatant *enemy;

    forced = 0;
    for (i = 0; i < 3; i++) {
        D_801E44EC = &D_801E44C8->records[i].pilot;
        if (D_801E44EC->flags32 & 0x800) {
            forced = 1;
        }
    }
    low_bit = 1;
    for (i = 0; i < 8; i++) {
        D_801E44C8->dropIds[i] = 0;
        bit = low_bit << i;
        if (!(D_801E44C8->defeated & bit)) {
            continue;
        }
        enemy = &D_801E44C8->records[i + 3];
        D_801E44EC = &enemy->pilot;
        if (rand() % 100 < enemy->field150[0] || forced == 1) {
            D_801E44C8->dropCategories[i] = enemy->field150[4];
            D_801E44C8->dropIds[i] = enemy->field150[2];
        } else if (rand() % 100 < enemy->field150[1]) {
            D_801E44C8->dropCategories[i] = enemy->field150[5];
            D_801E44C8->dropIds[i] = enemy->field150[3];
        }
    }
}
