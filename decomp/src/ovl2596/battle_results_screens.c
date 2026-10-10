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

/* The module's data opens with battle_results_fanfare_started (the fanfare flag, u8 0): the
 * padding after it holds a byte (0x04) that nothing reads, which a C
 * definition cannot leave, so it stays original data. */
INCLUDE_ORIGINAL(".data", battle_results_fanfare_started, 0x801E44C0, 4);
extern u8 battle_results_fanfare_started; /* the result fanfare has started */
GameData *battle_results_game_data_ptr = &game_data; /* 801E44C4 */
BattleWork *battle_results_work_ptr = &battle_work_area; /* 801E44C8 */
/* The module's uninitialized variables (the level gauge animation, the
 * growth data and the record being processed, the experience pools), zero
 * in the file: commons, which the original linker allocated each in a slot
 * of whole words (decomp/Makefile). */
s32 battle_results_gauge_start_value;               /* 801E44CC: gauge: start value */
s32 battle_results_gauge_unread_end_value;                 /* 801E44D0: end value */
s32 battle_results_gauge_change;                    /* 801E44D4: distance */
s32 battle_results_gauge_start_length;              /* 801E44D8: start length */
s32 battle_results_gauge_change_length;             /* 801E44DC: distance length */
u8 battle_results_gauge_change_color;               /* 801E44E0: bar colour */
u8 battle_results_gauge_arrow_glyph;                /* 801E44E4: arrow glyph */
GrowthFile *battle_results_growth_file;             /* 801E44E8: the growth data file */
CharacterRecord *battle_results_current_record;     /* 801E44EC: the record being processed: a combatant's or the game data's */
u32 battle_results_exp_pool_a;                      /* 801E44F0: experience pool for level A */
u32 battle_results_exp_pool_b;                      /* 801E44F4: and level B */
u8 battle_results_levels_before[3][2];              /* 801E44F8: each slot's levels A and B before the experience */

/* 801DE048: Queue every member card's glyph runs while the cards are shown. */
void battle_results_queue_member_cards(void) {
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

/* 801DE1C4: Queue the summary window's glyphs and bars, and the 8F panel. */
void battle_results_queue_summary_and_new_skill(void) {
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

/* 801DE408: Queue the spoils window's glyphs and item list. */
void battle_results_queue_spoils_window(void) {
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

/* 801DE594: Queue the result screens' primitives. */
void battle_results_queue_screens(void) {
    battle_results_queue_member_cards();
    battle_results_queue_summary_and_new_skill();
    battle_results_queue_spoils_window();
}

/* 801DE5C4: Build each present member's portrait glyphs. */
void battle_results_build_card_portraits(void) {
    s32 i;

    for (i = 0; i < 3; i++) {
        battle_member_cards[i]->runs[0].count = 0;
        if (battle_area.slots[i].field2 != 0x7F) {
            battle_member_cards[i]->runs[0].count += battle_build_glyph(i + 0xFC, &battle_member_cards[i]->portrait[battle_member_cards[i]->runs[0].count * 2], 0x20, i * 0x20 + 0x24);
        }
        battle_member_cards[i]->runs[0].buffer = battle_area.buffer;
    }
}

/* 801DE69C: Build each present member's card labels; with the first card's flag, add
 * the two marker glyphs, shaded red and green. */
void battle_results_build_card_labels(void) {
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

/* 801DEA18: Build each present member's four numbers as digit glyphs: two stats (three
 * and two digits) and two further values beside them. */
void battle_results_build_card_hp_and_ep(void) {
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

/* 801DEDC0: Build each present member's level glyphs, from the battle slots or (with
 * fromGameData) the game data; with the first card's flag also the second
 * level, and shade the two numbers red and green. */
void battle_results_build_card_levels(u8 fromGameData) {
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

/* 801DF270: Build each present member's first eight-digit number (and with the first
 * card's flag the second) as glyphs. */
void battle_results_build_card_exp_totals(void) {
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

/* 801DF4C0: Build each present member's seven-digit numbers as glyphs, like 801df270. */
void battle_results_build_card_exp_to_count(void) {
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

/* 801DF710: Set up a gauge bar's two primitives: a gradient from colour (0 pink,
 * 1 light green, 2 red, 3 blue) at the top to black. */
void battle_results_init_gauge_bar(POLY_G4 *bar, u8 colour) {
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

/* 801DF840: Shade count glyph parts from the given draw buffer red (or blue). */
void battle_results_shade_glyphs(POLY_FT4 *prims, u8 blue, u8 count, u8 buffer) {
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

/* 801DF910: Start the level gauge animation from one value to another out of max:
 * lengths on a 64-pixel scale, the colour and arrow for up or down. */
void battle_results_compute_gauge(u8 from, u8 to, s32 max) {
    battle_results_gauge_start_value = from;
    battle_results_gauge_unread_end_value = to;
    battle_results_gauge_change = to - from;
    battle_results_gauge_start_length = from * 100 / max * 0x1900 / 10000;
    if (battle_results_gauge_change >= 0) {
        battle_results_gauge_change_color = 2;
        battle_results_gauge_arrow_glyph = 0xE3;
    } else {
        battle_results_gauge_change_color = 3;
        battle_results_gauge_arrow_glyph = 0xE5;
        battle_results_gauge_change = from - to;
    }
    battle_results_gauge_change_length = battle_results_gauge_change * 100 / max * 0x1900 / 10000;
}

/* 801DFA38: The highest of a slot's seven entries in both byte tables. */
s32 battle_results_get_highest_stat(u8 slot) {
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

/* 801DFAA8: Build the summary window: the member's portrait title and the 27 text
 * glyphs, shading the marked ones with their colour. */
void battle_results_build_summary_text(u8 member) {
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

/* 801DFD58: Build the summary's first member value (three digits); clear the other
 * summary number runs. */
void battle_results_build_summary_max_hp(u8 member) {
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

/* 801DFE6C: Build the summary's second member value (two digits). */
void battle_results_build_summary_max_ep(u8 member) {
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

/* 801DFF50: Build the summary's change of the member's first stat since the battle
 * began: an up or down arrow and the difference, shaded red. */
void battle_results_build_summary_max_hp_change(u8 member) {
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

/* 801E0184: Build the summary's change of the member's second stat, like 801dff50. */
void battle_results_build_summary_max_ep_change(u8 member) {
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

/* 801E03B8: Build the summary's member numbers and their changes. */
void battle_results_build_summary_numbers(u8 member) {
    battle_results_build_summary_max_hp(member);
    battle_results_build_summary_max_ep(member);
    battle_results_build_summary_max_hp_change(member);
    battle_results_build_summary_max_ep_change(member);
}

/* 801E03FC: Build the member's seven gauge rows: for each, the value before and after
 * the battle out of the highest (801dfa38) as a bar and its change bar, the
 * value, and when it changed an arrow and the change shaded by direction.
 * The value's digits are entries 23-25 of battle_split_decimal_digits's digit buffer. */
void battle_results_build_summary_stat_gauges(u8 member) {
    s32 max;
    s32 i;
    s32 j;
    s32 digit;
    s32 left;
    s32 n;
    s32 k;
    s32 top;
    s32 bottom;

    max = battle_results_get_highest_stat(member);
    for (i = 0; i < 7; i++) {
        battle_summary_window_prims->rowACount[i] = 0;
        battle_summary_window_prims->rowBCount[i] = 0;
        battle_results_compute_gauge(battle_work_area.savedStats[member][i], battle_work_area.resultStats[member][i], max);
        battle_results_init_gauge_bar(battle_summary_window_prims->barA[i], 0);
        battle_results_init_gauge_bar(battle_summary_window_prims->barB[i], battle_results_gauge_change_color);
        top = i * 8 + 0x92;
        bottom = i * 8 + 0x98;
        (battle_summary_window_prims->barA[i] + battle_area.buffer)->x0 = 0x78;
        (battle_summary_window_prims->barA[i] + battle_area.buffer)->y0 = top;
        (battle_summary_window_prims->barA[i] + battle_area.buffer)->x1 = battle_results_gauge_start_length + 0x78;
        (battle_summary_window_prims->barA[i] + battle_area.buffer)->y1 = top;
        (battle_summary_window_prims->barA[i] + battle_area.buffer)->x2 = 0x78;
        (battle_summary_window_prims->barA[i] + battle_area.buffer)->y2 = bottom;
        (battle_summary_window_prims->barA[i] + battle_area.buffer)->x3 = battle_results_gauge_start_length + 0x78;
        (battle_summary_window_prims->barA[i] + battle_area.buffer)->y3 = bottom;
        if (battle_results_gauge_change_color == 2) {
            left = battle_results_gauge_start_length + 0x78;
        } else {
            left = battle_results_gauge_start_length + 0x78 - battle_results_gauge_change_length;
        }
        (battle_summary_window_prims->barB[i] + battle_area.buffer)->x0 = left;
        (battle_summary_window_prims->barB[i] + battle_area.buffer)->y0 = top;
        (battle_summary_window_prims->barB[i] + battle_area.buffer)->x1 = left + battle_results_gauge_change_length;
        (battle_summary_window_prims->barB[i] + battle_area.buffer)->y1 = top;
        (battle_summary_window_prims->barB[i] + battle_area.buffer)->x2 = left;
        (battle_summary_window_prims->barB[i] + battle_area.buffer)->y2 = bottom;
        (battle_summary_window_prims->barB[i] + battle_area.buffer)->x3 = left + battle_results_gauge_change_length;
        (battle_summary_window_prims->barB[i] + battle_area.buffer)->y3 = bottom;
        battle_summary_window_prims->barBuffer[i] = battle_area.buffer;
        battle_split_decimal_digits(battle_results_gauge_start_value);
        for (j = 0; j < 3; j++) {
            k = j + 23;
            digit = battle_decimal_digits_minus_17[k];
            if (digit != 0xFF) {
                battle_summary_window_prims->rowACount[i] += battle_build_glyph(digit, &battle_summary_window_prims->rowA[i][battle_summary_window_prims->rowACount[i] * 2], j * 8 + 0xB8, i * 8 + 0x90);
            }
        }
        battle_summary_window_prims->rowABuffer[i] = battle_area.buffer;
        if (battle_results_gauge_change != 0) {
            battle_summary_window_prims->rowBCount[i] = battle_build_glyph(battle_results_gauge_arrow_glyph, battle_summary_window_prims->rowB[i], 0xD8, i * 8 + 0x90);
            battle_split_decimal_digits(battle_results_gauge_change);
            n = 0;
            for (j = 0; j < 3; j++) {
                digit = battle_decimal_digits_plus_6[j];
                if (digit != 0xFF) {
                    battle_summary_window_prims->rowBCount[i] += battle_build_glyph(digit, &battle_summary_window_prims->rowB[i][battle_summary_window_prims->rowBCount[i] * 2], n * 8 + 0xE0, i * 8 + 0x90);
                    n++;
                }
            }
            battle_results_shade_glyphs(battle_summary_window_prims->rowB[i], battle_results_gauge_change_color - 2, battle_summary_window_prims->rowBCount[i], battle_area.buffer);
            battle_summary_window_prims->rowBBuffer[i] = battle_area.buffer;
        }
    }
}

/* 801E09C0: Play effect id of the system effect bank. */
void battle_results_play_sound_effect(u8 id) {
    sound_play_effect((sprite_script_sound_bank->bank << 16) | id);
}

/* 801E09F4: Start the result fanfare's three effects once. */
void battle_results_start_fanfare(void) {
    if (battle_results_fanfare_started == 0) {
        mode_result_fanfare_started = 1;
        battle_results_play_sound_effect(0x5C);
        battle_results_play_sound_effect(0x5D);
        battle_results_play_sound_effect(0x5E);
        battle_results_fanfare_started = 1;
    }
}

/* 801E0A4C: Start the fanfare and run battle frames until Cross is pressed. */
void battle_results_wait_for_cross_with_fanfare(void) {
    battle_results_start_fanfare();
    battle_wait_frame();
    battle_ui->waitingCross = 1;
    while (battle_pressed_key != 4) {
        battle_wait_frame();
    }
    battle_ui->waitingCross = 0;
}

/* 801E0ACC: Show the skills the member learnt in the battle, one at a time: the
 * summary title, then each new counter skill and each new level skill's
 * name with its mark, waiting for Cross after each. */
void battle_results_show_new_deathblows_and_arts(u8 member) {
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
            battle_results_wait_for_cross_with_fanfare();
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
            battle_results_wait_for_cross_with_fanfare();
        }
    }
    battle_ui->showSkill = 0;
}


/* 801E1044: Lay out the summary's seven-glyph label (2d30). */
void battle_results_build_spoils_label(void) {
    s32 i;

    for (i = 0; i < 7; i++) {
        battle_summary_window_prims->count2D30 += battle_build_glyph(battle_spoils_label_glyphs[i], battle_summary_window_prims->glyphs2D30[battle_summary_window_prims->count2D30], battle_spoils_label_x[i], battle_spoils_label_y[i]);
    }
    battle_summary_window_prims->buffer2D30 = battle_area.buffer;
}

/* 801E10F8: Build the spoils window's numbers: the experience (six digits) and the
 * party gold (nine digits). */
void battle_results_build_spoils_exp_and_gold(u32 experience) {
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

/* 801E126C: Build the spoils window's two icons. */
void battle_results_build_spoils_icons(void) {
    s32 *buffer;

    battle_init_text_quad_pair(battle_summary_window_prims->glyphs3410[0], 0, 2);
    battle_init_text_quad_pair(battle_summary_window_prims->glyphs3410[1], 1, 2);
    buffer = &battle_area.buffer;
    battle_quad_place_text_row(&battle_summary_window_prims->glyphs3410[0][*buffer], 0x20, 0x20, battle_spoils_icon_cells[2], battle_spoils_icon_cells[3], battle_spoils_icon_cells[0]);
    battle_quad_place_text_row(&battle_summary_window_prims->glyphs3410[1][*buffer], 0xB8, 0x40, battle_spoils_icon_cells[6], battle_spoils_icon_cells[7], battle_spoils_icon_cells[4]);
    battle_summary_window_prims->buffer3410 = *buffer;
}

/* 801E1370: Add count of item id to an inventory list of size entries (ids and
 * counts): stack onto the item (at most 99) or take the first free entry;
 * a full list drops the item. */
void battle_results_add_to_inventory_list(u8 id, u8 count, u8 *ids, u8 *counts, u8 size) {
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

/* 801E1444: Add eight drops (ids, counts and inventory list categories) to the
 * inventory. */
void battle_results_add_drops_to_inventory(u8 *ids, u8 *counts, u8 *categories) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (ids[i] != 0) {
            switch (categories[i]) {
            case 0:
                battle_results_add_to_inventory_list(ids[i], counts[i], game_data.weaponIds, game_data.weaponCounts, 100);
                break;
            case 1:
                battle_results_add_to_inventory_list(ids[i], counts[i], game_data.accessoryIds, game_data.accessoryCounts, 200);
                break;
            case 2:
                battle_results_add_to_inventory_list(ids[i], counts[i], game_data.itemIds, game_data.itemCounts, 150);
                break;
            case 3:
                battle_results_add_to_inventory_list(ids[i], counts[i], game_data.gearPartIds, game_data.gearPartCounts, 100);
                break;
            case 4:
                battle_results_add_to_inventory_list(ids[i], counts[i], game_data.gearAccessoryIds, game_data.gearAccessoryCounts, 150);
                break;
            }
        }
    }
}

/* 801E1590: Collect the rolled drops into eight distinct (category, id) entries
 * with their counts. */
void battle_results_collect_drops(u8 *ids, u8 *counts, u8 *categories) {
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

/* 801E1690: Build the spoils window's item list: collect the drops, render each
 * item's name into VRAM with its count, then add the drops to the
 * inventory. */
void battle_results_build_spoils_item_list(void) {
    u8 ids[8];
    u8 categories[8];
    u8 counts[8];
    RECT rect;
    void *text[8];
    void **names; /* the text buffers as the name loop fills them */
    s32 i;
    s32 count;
    s32 width;

    battle_results_collect_drops(ids, counts, categories);
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
    battle_results_add_drops_to_inventory(ids, counts, categories);
    for (i = 0; i < count; i++) {
        heap_free(text[i]);
    }
    battle_summary_window_prims->listCount = count;
    battle_summary_window_prims->listBuffer = battle_area.buffer;
}

/* 801E196C: Show the member cards over six frames, then wait for Cross. */
void battle_results_show_member_cards(void) {
    u32 step;
    u8 building;

    step = 0;
    building = 1;
    do {
        battle_wait_frame();
        switch (step) {
        case 0:
            battle_results_build_card_portraits();
            battle_ui->showCards = 1;
            battle_turn_state->eventsDone = 0;
            break;
        case 1:
            battle_results_build_card_labels();
            break;
        case 2:
            battle_results_build_card_hp_and_ep();
            break;
        case 3:
            battle_results_build_card_levels(0);
            break;
        case 4:
            battle_results_build_card_exp_totals();
            break;
        case 5:
            battle_results_build_card_exp_to_count();
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

/* 801E1AA4: Wait for Cross on the first member card (repeating the prompt sound),
 * then take each member's two values from the game data and rebuild the
 * summary rows. */
void battle_results_count_card_exp(void) {
    s32 i;

    battle_wait_frame();
    battle_ui->waitingCross = 1;
    battle_member_cards[0]->counting = 1;
    while (battle_member_cards[0]->counting != 0) {
        if (battle_pressed_key == 4) {
            break;
        }
        battle_results_play_sound_effect(0x5B);
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
    battle_results_build_card_exp_totals();
    battle_results_build_card_exp_to_count();
}

/* 801E1C10: After Cross, show each present member's summary window when a stat
 * changed (waiting for Cross), then its skill results (801e0acc). */
void battle_results_show_level_up_summaries(void) {
    s32 i;
    u8 shown;
    u8 member;

    battle_results_build_card_levels(1);
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
                battle_results_build_summary_text(member);
                battle_results_build_summary_numbers(member);
                battle_results_build_summary_stat_gauges(member);
                battle_ui->showSummary = 1;
                battle_results_start_fanfare();
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
            battle_results_show_new_deathblows_and_arts(i);
            battle_ui->waitingCross = 0;
            battle_ui->windows[1] = 0;
        }
    }
}

/* 801E1E10: Show the spoils window (experience, gold, items) until Cross. */
void battle_results_show_spoils_window(u32 experience) {
    battle_ui->showCards = 0;
    battle_ui->showSummary = 0;
    battle_ui->showSkill = 0;
    battle_wait_frame();
    battle_window_open(0, 0x18, 0x18, 0x90, 0xA0, 0, 1);
    battle_window_open(2, 0xB0, 0x38, 0x70, 0x38, 0, 1);
    battle_wait_frame();
    battle_results_build_spoils_label();
    battle_results_build_spoils_exp_and_gold(experience);
    battle_results_build_spoils_icons();
    battle_results_build_spoils_item_list();
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

/* 801E1FB8: The battle results: allocate the member cards and the summary, show the
 * cards, the summaries and the spoils, then release them. */
void battle_results_run_screens(u32 experience) {
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
    battle_results_show_member_cards();
    battle_results_count_card_exp();
    battle_results_show_level_up_summaries();
    battle_results_show_spoils_window(experience);
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

/* 801E211C: Hide the battle windows and reload the results resources: archive file
 * 2 of directory 0x10 (its items 1-4: text, a table, the glyph sprites and
 * the portraits). */
void battle_results_load_resources(void) {
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

/* 801E2280: Total the experience and gold of the defeated enemies, add the gold (up
 * to 9999999), clear empty party slots, grant the rewards and run the
 * result screens. */
void battle_results_grant_rewards(void) {
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
    battle_results_grant_exp_skills_and_drops();
    if (battle_skip_result_screens == 0 && !(formation_active.flags & 8)) {
        battle_results_run_screens(gold);
    }
}

/* 801E24B0: Write the battle item counts back to inventory list 2. */
void battle_results_write_back_item_counts(void) {
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

/* 801E252C: Leave the battle: reload the resources, pick the next mode, write the
 * items back, grant the rewards (unless the battle was escaped or they are
 * skipped), release the battle's blocks and windows and reset the sound. */
void battle_results_leave_battle(void) {
    s32 i;
    u8 *outcome;

    battle_results_load_resources();
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
    battle_results_write_back_item_counts();
    outcome = &battle_area.outcome;
    if (!(*outcome & 0xC0) && *outcome != 0x21 && battle_exit_requested == 0 && mode_result_code != 3) {
        battle_load_wave_bank_5();
        battle_results_grant_rewards();
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

/* 801E2794: Grant the battle rewards unless the whole party is knocked out; then, when
 * character 3 pilots gear 0x12, set his skill flags 1A to 0x4000, with 0x8000
 * too when gear 12 has a part kind 5 amount. */
void battle_results_grant_exp_skills_and_drops(void) {
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
        battle_results_growth_file = battle_results_work_ptr->growth;
        battle_results_distribute_exp();
        battle_results_learn_skills();
        battle_results_advance_tiers();
        battle_results_unlock_at_levels_50_60_70();
        battle_results_write_party_to_game_data();
        battle_results_roll_drops();
        if (game_data.characters[3].gearId == 0x12) {
            extra = game_data.gears[12].field4F;
            game_data.skills[3].flags1A = 0x4000;
            if (extra != 0) {
                game_data.skills[3].flags1A = 0xC000;
            }
        }
    }
}

/* 801E2888: Write each party member's HP, EP, counters and gear HP and fuel back to
 * the game data, clamped to their maximums (HP 1 when knocked out, gear HP a
 * tenth of the maximum when destroyed). */
void battle_results_write_party_to_game_data(void) {
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
        record = &battle_results_work_ptr->records[slot];
        character = &battle_results_game_data_ptr->characters[record->pilot.characterId];
        gear = &battle_results_game_data_ptr->gears[record->pilot.gearId];
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

/* 801E2ACC: Distribute the experience won: party members that stand share it (less
 * the penalty), every other character gets a reserve share of a third;
 * then record each slot's level gains and result stats. */
void battle_results_distribute_exp(void) {
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
        battle_results_current_record = &battle_results_work_ptr->records[i].pilot;
        if (battle_results_current_record->status7C & 0xC000) {
            absent++;
            slots[battle_results_current_record->characterId] = 0xFF;
        } else {
            slots[battle_results_current_record->characterId] = i;
        }
        battle_results_levels_before[i][0] = battle_results_current_record->level;
        battle_results_levels_before[i][1] = battle_results_current_record->level2;
    }
    experience = battle_results_work_ptr->experience;
    if (battle_results_work_ptr->penalty != 0) {
        experience -= (experience / 4) * battle_results_work_ptr->penalty;
    }
    for (i = 0; i < 11; i++) {
        if (slots[i] == 0xFF) {
            continue;
        }
        battle_results_current_record = &battle_results_game_data_ptr->characters[i];
        if (slots[i] < 3) {
            battle_results_split_exp_into_pools(experience / (3 - absent), slots[i], 0);
        } else {
            battle_results_split_exp_into_pools(experience / 3, 0xFF, 1);
        }
        battle_results_add_exp_and_level_up();
    }
    for (i = 0; i < 3; i++) {
        if (battle_party_character_ids[i] == 0xFF) {
            continue;
        }
        battle_results_current_record = &battle_results_game_data_ptr->characters[battle_party_character_ids[i]];
        battle_results_work_ptr->levelGains[i][0] = battle_results_current_record->level - battle_results_levels_before[i][0];
        battle_results_work_ptr->levelGains[i][1] = battle_results_current_record->level2 - battle_results_levels_before[i][1];
        if (battle_results_current_record->characterId == 4) {
            battle_results_work_ptr->resultStats[i][0] = battle_results_current_record->entries[0].value4 + battle_results_current_record->entries[3].value4;
        } else {
            battle_results_work_ptr->resultStats[i][0] = battle_results_current_record->attack + battle_results_current_record->entries[0].value4;
        }
        battle_results_work_ptr->resultStats[i][1] = battle_results_current_record->field5E;
        battle_results_work_ptr->resultStats[i][2] = battle_results_current_record->defense + battle_results_current_record->bodyDefense;
        battle_results_work_ptr->resultStats[i][3] = battle_results_current_record->field5F;
        battle_results_work_ptr->resultStats[i][4] = battle_results_current_record->ether;
        battle_results_work_ptr->resultStats[i][5] = battle_results_current_record->etherDefense;
        battle_results_work_ptr->resultStats[i][6] = battle_results_current_record->speed;
    }
}

/* 801E2EB0: Split a slot's experience into the level A and B pools by the record's
 * weights (three quarters each for a reserve member, all of it with option
 * 0x8000), at least 1, raised by half by flags 0x2000/0x1000; kept per slot. */
void battle_results_split_exp_into_pools(u32 experience, s16 slot, s16 reserve) {
    s16 weightA;
    s16 weightB;

    if (reserve == 1) {
        experience = experience * 3 / 4;
        battle_results_exp_pool_a = experience;
        battle_results_exp_pool_b = experience;
        return;
    }
    weightA = battle_results_work_ptr->records[slot].expWeightA;
    weightB = battle_results_work_ptr->records[slot].expWeightB;
    if (weightA < 2) {
        weightA = 1;
    }
    if (weightB < 2) {
        weightB = 1;
    }
    battle_results_exp_pool_a = experience * weightA / (weightA + weightB);
    battle_results_exp_pool_b = experience * weightB / (weightA + weightB);
    if (battle_results_game_data_ptr->flags & 0x8000) {
        battle_results_exp_pool_a = experience;
        battle_results_exp_pool_b = experience;
    }
    if (battle_results_exp_pool_a == 0) {
        battle_results_exp_pool_a = 1;
    }
    if (battle_results_exp_pool_b == 0) {
        battle_results_exp_pool_b = 1;
    }
    if (battle_results_current_record->flags32 & 0x2000) {
        battle_results_exp_pool_a += battle_results_exp_pool_a >> 1;
        battle_results_exp_pool_b += battle_results_exp_pool_b >> 1;
    }
    if (battle_results_current_record->flags32 & 0x1000) {
        battle_results_exp_pool_b += battle_results_exp_pool_b >> 1;
    }
    battle_results_work_ptr->toCount[slot][0] = battle_results_exp_pool_a;
    battle_results_work_ptr->toCount[slot][1] = battle_results_exp_pool_b;
}

/* 801E308C: Add the experience pools to the current record's totals and gain levels
 * A and B while the pools reach the next level (none past 99 with option
 * 0x8000, and nothing more at 99). */
void battle_results_add_exp_and_level_up(void) {
    s32 rest;

    battle_results_current_record->expTotalA += battle_results_exp_pool_a;
    battle_results_current_record->expTotalB += battle_results_exp_pool_b;
    if (battle_results_current_record->level == 99 && (battle_results_game_data_ptr->flags & 0x8000)) {
        battle_results_exp_pool_a = 0;
    }
    if (battle_results_current_record->level2 == 99 && (battle_results_game_data_ptr->flags & 0x8000)) {
        battle_results_exp_pool_b = 0;
    }
    if (battle_results_current_record->level == 99) {
        battle_results_exp_pool_a = 0;
    }
    if (battle_results_current_record->level2 == 99) {
        battle_results_exp_pool_b = 0;
    }
    rest = battle_results_current_record->expNextA - battle_results_exp_pool_a;
    if (rest > 0) {
        battle_results_current_record->expNextA = rest;
    } else {
        do {
            if (++battle_results_current_record->level >= 100 && (battle_results_game_data_ptr->flags & 0x8000)) {
                battle_results_current_record->level--;
            }
            battle_results_current_record->expNextA = battle_results_growth_file->experience[battle_results_current_record->level - 1];
            battle_results_grow_level_a_stats();
            rest += battle_results_current_record->expNextA;
        } while (rest <= 0);
    }
    battle_results_current_record->expNextA = rest;
    rest = battle_results_current_record->expNextB - battle_results_exp_pool_b;
    if (rest > 0) {
        battle_results_current_record->expNextB = rest;
    } else {
        do {
            if (++battle_results_current_record->level2 >= 100 && (battle_results_game_data_ptr->flags & 0x8000)) {
                battle_results_current_record->level2--;
            }
            battle_results_current_record->expNextB = battle_results_growth_file->experience[battle_results_current_record->level2 - 1];
            battle_results_grow_level_b_stats();
            rest += battle_results_current_record->expNextB;
        } while (rest <= 0);
    }
    battle_results_current_record->expNextB = rest;
}

/* 801E335C: Level A growth of the current record: max HP, then stats 58, 59, 5e
 * and 5f toward the growth data's targets for its level range. */
void battle_results_grow_level_a_stats(void) {
    u8 high;
    u8 cap;
    u8 level;

    high = 0;
    cap = 100;
    if (battle_results_current_record->level >= 100) {
        high = 1;
        cap = 200;
    }
    level = battle_results_current_record->level;
    battle_results_current_record->maxHp = battle_results_grow_max_hp(battle_results_current_record->maxHp, battle_results_current_record->level);
    battle_results_current_record->attack = battle_results_grow_stat(battle_results_current_record->attack,
        battle_results_growth_file->characters[battle_results_current_record->characterId].statTargets[0][high], cap, level);
    battle_results_current_record->defense = battle_results_grow_stat(battle_results_current_record->defense,
        battle_results_growth_file->characters[battle_results_current_record->characterId].statTargets[1][high], cap, level);
    battle_results_current_record->field5E = battle_results_grow_stat(battle_results_current_record->field5E,
        battle_results_growth_file->characters[battle_results_current_record->characterId].statTargets[2][high], cap, level);
    battle_results_current_record->field5F = battle_results_grow_stat(battle_results_current_record->field5F,
        battle_results_growth_file->characters[battle_results_current_record->characterId].statTargets[3][high], cap, level);
}

/* 801E3500: Level B growth of the current record: max EP, then stats 5b and 5c
 * toward the growth data's targets for its level range. */
void battle_results_grow_level_b_stats(void) {
    u8 high;
    u8 cap;
    u8 level;

    high = 0;
    cap = 100;
    if (battle_results_current_record->level2 >= 100) {
        high = 1;
        cap = 200;
    }
    level = battle_results_current_record->level2;
    battle_results_current_record->maxEp = battle_results_grow_max_ep(battle_results_current_record->maxEp, battle_results_current_record->level2);
    battle_results_current_record->ether = battle_results_grow_stat(battle_results_current_record->ether,
        battle_results_growth_file->characters[battle_results_current_record->characterId].statTargets[4][high], cap, level);
    battle_results_current_record->etherDefense = battle_results_grow_stat(battle_results_current_record->etherDefense,
        battle_results_growth_file->characters[battle_results_current_record->characterId].statTargets[5][high], cap, level);
}

/* 801E3610: Grow a stat by 0 or 1: the chance is the share of the distance to the
 * target left over the levels to the cap. Capped at 200. */
/* Grow a stat by 0 or 1: the chance is the share of the distance to the
 * target left over the levels to the cap. Capped at 200. */
u8 battle_results_grow_stat(u8 stat, u8 target, u8 cap, u8 level) {
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

/* 801E3700: Grow max HP by a random share of the distance to the growth data's
 * target for the level range, at least 2. Capped at 999. */
u16 battle_results_grow_max_hp(u16 maxHp, u8 level) {
    s16 gain;
    u16 hp;
    u16 result;
    s32 top = 99; /* the first range's top level */

    hp = maxHp;
    if (level < 100) {
        gain = rand() % 100 * (battle_results_growth_file->characters[battle_results_current_record->characterId].maxHpTargets[0] - (level - top) - hp)
            / ((100 - level) * 100) + 2;
        if (gain < 0) {
            result = hp;
        } else {
            result = gain + maxHp;
        }
    } else {
        gain = rand() % 100 * ((battle_results_growth_file->characters[battle_results_current_record->characterId].maxHpTargets[1] - hp)
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

/* 801E38CC: Grow max EP by 0 or 1 toward the growth data's target for the level
 * range (as battle_results_grow_stat). Capped at 99. */
u8 battle_results_grow_max_ep(u8 maxEp, u8 level) {
    u8 target;
    u8 cap;
    s32 random;
    s32 share;
    u8 grow;

    if (level < 100) {
        target = battle_results_growth_file->characters[battle_results_current_record->characterId].maxEpTargets[0];
        cap = 99;
    } else {
        target = battle_results_growth_file->characters[battle_results_current_record->characterId].maxEpTargets[1];
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

/* 801E3A18: Learn skills for each party slot that is not knocked out: a counter skill
 * (not characters 7 and 8), a level skill (not 10), the unlocks, and the
 * special cases of characters 8 and 7. */
void battle_results_learn_skills(void) {
    u8 slot;
    u8 learnt;

    for (slot = 0; slot < 3; slot++) {
        battle_results_current_record = &battle_results_work_ptr->records[slot].pilot;
        if (battle_results_current_record->status7C & 0x8000) {
            continue;
        }
        switch (battle_results_current_record->characterId) {
        case 7:
        case 8:
            break;
        default:
            learnt = battle_results_learn_deathblow(battle_results_current_record->characterId);
            if (learnt != 0) {
                battle_results_work_ptr->learntCounter[slot] = learnt;
            }
            break;
        }
        if (battle_results_current_record->characterId != 10) {
            learnt = battle_results_learn_art(battle_results_current_record->characterId);
            if (learnt != 0) {
                battle_results_work_ptr->learntLevel[slot] = learnt;
            }
        }
        switch (battle_results_current_record->characterId) {
        case 7:
        case 8:
        case 10:
            break;
        default:
            battle_results_unlock_by_known_deathblows(battle_results_current_record->characterId);
            break;
        }
        switch (battle_results_current_record->characterId) {
        case 8:
        case 9:
        case 10:
            break;
        default:
            battle_results_unlock_by_known_arts(battle_results_current_record->characterId);
            break;
        }
        if (battle_results_current_record->characterId == 8) {
            battle_results_unlock_character8_by_level();
        }
        if (battle_results_current_record->characterId == 7) {
            battle_results_derive_character7_gear_stats();
        }
    }
}

/* 801E3BE0: Learn the first unknown counter skill (of 7, or 13 with option 0x4000)
 * whose level is reached and whose seven counter requirements the current
 * record meets; stop at the first whose level is not reached. Returns its
 * index, or 0. */
u8 battle_results_learn_deathblow(u8 id) {
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
        if (battle_results_growth_file->characters[id].counterLevels[j] > game_data.characters[id].level) {
            break;
        }
        for (k = 0; k < 7; k++) {
            if (battle_results_growth_file->characters[id].requirements[j][k] > battle_results_current_record->useCounts[k]) {
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

/* 801E3D54: Learn the first of the character's twelve level skills whose level is
 * reached and which is not yet known. Returns its number (1-12), or 0. */
u8 battle_results_learn_art(u8 id) {
    u32 bit;
    u8 k;
    u8 level;
    u32 known;

    bit = 0x8000;
    for (k = 0; k < 12; k++, bit >>= 1) {
        level = battle_results_growth_file->characters[id].levelSkills[k];
        if (level == 0xFF) {
            return 0;
        }
        if (game_data.characters[id].level >= level) {
            known = battle_results_game_data_ptr->skills[id].levelSkills;
            if (!(bit & known)) {
                battle_results_game_data_ptr->skills[id].levelSkills = bit | known;
                return k + 1;
            }
        }
    }
    return 0;
}

/* 801E3E14: For each of the character's nine unlock entries whose counter skill is
 * known, set the matching unlock bit (from bit 3). */
void battle_results_unlock_by_known_deathblows(u8 id) {
    u8 k;
    u8 entry;

    for (k = 0; k < 9; k++) {
        entry = battle_results_growth_file->characters[id].unlocksA[k];
        if (entry == 0xFF) {
            return;
        }
        if (battle_results_game_data_ptr->skills[id].counterSkills & (0x8000 >> (entry - 1))) {
            battle_results_game_data_ptr->skills[id].unlocksA |= 0x8000 >> (k + 3);
        }
    }
}

/* 801E3EA4: Character 8's nine level entries: learn each one its level reaches. */
void battle_results_unlock_character8_by_level(void) {
    u8 k;
    u8 level;
    u32 known;
    s32 bit;

    for (k = 0; k < 9; k++) {
        level = battle_results_growth_file->characters[8].unlocksA[k];
        if (level == 0xFF) {
            return;
        }
        if (game_data.characters[8].level >= level) {
            known = battle_results_game_data_ptr->skills[8].unlocksA;
            bit = 0x1000 >> k;
            if (!(known & bit)) {
                battle_results_game_data_ptr->skills[8].unlocksA = bit | known;
            }
        }
    }
}

/* 801E3F28: For each of the character's thirteen second unlock entries whose level
 * skill is known, set the matching unlock bit. */
void battle_results_unlock_by_known_arts(u8 id) {
    u8 k;
    u8 entry;

    for (k = 0; k < 13; k++) {
        entry = battle_results_growth_file->characters[id].unlocksB[k];
        if (entry == 0) {
            return;
        }
        if (battle_results_game_data_ptr->skills[id].levelSkills & (0x8000 >> (entry - 1))) {
            battle_results_game_data_ptr->skills[id].unlocksB |= 0x8000 >> k;
        }
    }
}

/* 801E3FB0: Character 7's derived values from the current record's max HP and
 * attack. */
void battle_results_derive_character7_gear_stats(void) {
    GameData *game = battle_results_game_data_ptr;

    game->gears[7].maxHp = battle_results_current_record->maxHp * 200;
    game->gears[7].attack = battle_results_current_record->attack / 5 + 1;
    game->gears[7].bodyDefense = battle_results_current_record->maxHp * 10;
    game->gears[7].armor = battle_results_current_record->maxHp * 10;
}

/* 801E403C: Advance each character's tier: 3, 4 and 5 at the growth data's tier
 * levels, 6 to 7 at level 50 with option 0x4000. */
void battle_results_advance_tiers(void) {
    u8 id;
    CharacterRecord *character;
    CharacterSkills *skills;

    for (id = 0; id < 11; id++) {
        character = &battle_results_game_data_ptr->characters[id];
        skills = &battle_results_game_data_ptr->skills[id];
        if (skills->tier == 7) {
            continue;
        }
        switch (skills->tier) {
        case 3:
            if (character->level >= battle_results_growth_file->characters[id].tierLevels[0]) {
                skills->tier = 4;
            }
            break;
        case 4:
            if (character->level >= battle_results_growth_file->characters[id].tierLevels[1]) {
                skills->tier = 5;
            }
            break;
        case 5:
            if (character->level >= battle_results_growth_file->characters[id].tierLevels[2]) {
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

/* 801E41B4: Levels 50, 60 and 70 set unlock bits 8, 4 and 2 of each party member. */
void battle_results_unlock_at_levels_50_60_70(void) {
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

/* 801E42C4: Roll one drop per defeated enemy: the first at its chance (always when a
 * party member has flag 0x800), else the second at its chance. */
void battle_results_roll_drops(void) {
    u8 i;
    u8 forced;
    s32 low_bit;
    s32 bit;
    Combatant *enemy;

    forced = 0;
    for (i = 0; i < 3; i++) {
        battle_results_current_record = &battle_results_work_ptr->records[i].pilot;
        if (battle_results_current_record->flags32 & 0x800) {
            forced = 1;
        }
    }
    low_bit = 1;
    for (i = 0; i < 8; i++) {
        battle_results_work_ptr->dropIds[i] = 0;
        bit = low_bit << i;
        if (!(battle_results_work_ptr->defeated & bit)) {
            continue;
        }
        enemy = &battle_results_work_ptr->records[i + 3];
        battle_results_current_record = &enemy->pilot;
        if (rand() % 100 < enemy->field150[0] || forced == 1) {
            battle_results_work_ptr->dropCategories[i] = enemy->field150[4];
            battle_results_work_ptr->dropIds[i] = enemy->field150[2];
        } else if (rand() % 100 < enemy->field150[1]) {
            battle_results_work_ptr->dropCategories[i] = enemy->field150[5];
            battle_results_work_ptr->dropIds[i] = enemy->field150[3];
        }
    }
}
