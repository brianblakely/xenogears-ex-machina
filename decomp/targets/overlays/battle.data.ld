/* Names the battle source uses for addresses inside a C data object, each
 * from that object's linked address. battle.c indexes the timer reload table
 * D_800C31EC (maximum AP 3-7) by the maximum AP itself, from three 8-byte
 * rows before it, and the combo step table D_800C34B4 by the AP paid (1-3),
 * from one byte before it: GCC 2.6.3 forms that base with `la` only for a
 * declared array (D_800C34B4[step][paid - 1] puts the -1 in the load). */
D_800C31D4 = D_800C31EC - 0x18;
D_800C34B3 = D_800C34B4 - 0x1;

/* The same for the commons (battle_common.c). The units address parts of the
 * battle area D_800C3EB0 (area.h), of its battle work (combatant.h) and of
 * the camera, image animation and message entries by names of their own,
 * with views of their own (BattleDraw from .ot, ActionCommit from
 * .work.targetMask). */
D_800C3EB4 = D_800C3EB0 + 0x4;    /* .slots */
D_800C3FE8 = D_800C3EB0 + 0x138;  /* .events */
D_800C4000 = D_800C3EB0 + 0x150;  /* .events[0] + 0x18 */
D_800C48E8 = D_800C3EB0 + 0xA38;  /* .knockedOut */
D_800C48EA = D_800C3EB0 + 0xA3A;  /* .outcome */
D_800C4922 = D_800C3EB0 + 0xA72;  /* .path[9] onwards */
D_800C4924 = D_800C3EB0 + 0xA74;
D_800C4928 = D_800C3EB0 + 0xA78;
D_800C4929 = D_800C3EB0 + 0xA79;
D_800C492A = D_800C3EB0 + 0xA7A;
D_800C4A39 = D_800C3EB0 + 0xB89;  /* .buffers[0].drawEnv.r0 */
D_800CCB04 = D_800C3EB0 + 0x8C54; /* .ot */
D_800CCB34 = D_800C3EB0 + 0x8C84; /* .buffer */
D_800CCB94 = D_800C3EB0 + 0x8CE4;
D_800CCC58 = D_800C3EB0 + 0x8DA8; /* .field8DA8 */
D_800CCC5C = D_800C3EB0 + 0x8DAC; /* .frameTicks */
D_800CCCE8 = D_800C3EB0 + 0x8E38; /* .work */
D_800D2200 = D_800CCCE8 + 0x5518; /* .work.lists + 0x20: the item effects */
D_800D2C0C = D_800CCCE8 + 0x5F24; /* .work.gearHud */
D_800D2C2A = D_800CCCE8 + 0x5F42;
D_800D2C30 = D_800CCCE8 + 0x5F48;
D_800D2C32 = D_800CCCE8 + 0x5F4A;
D_800D2C34 = D_800CCCE8 + 0x5F4C;
D_800D2C35 = D_800CCCE8 + 0x5F4D;
D_800D2C36 = D_800CCCE8 + 0x5F4E;
D_800D2C38 = D_800CCCE8 + 0x5F50;
D_800D2C3A = D_800CCCE8 + 0x5F52;
D_800D2C54 = D_800CCCE8 + 0x5F6C; /* .work.damage */
D_800D2C60 = D_800CCCE8 + 0x5F78; /* .work.damage[3] */
D_800D2C88 = D_800CCCE8 + 0x5FA0; /* .work.resultCode */
D_800D2C8B = D_800CCCE8 + 0x5FA3; /* .work.resultCode[3] */
D_800D2C94 = D_800CCCE8 + 0x5FAC; /* .work.targetMask */
D_800D2C9E = D_800CCCE8 + 0x5FB6; /* .work.revived */
D_800D30A0 = D_800D309C + 0x4;    /* camera .eye, .target */
D_800D30B0 = D_800D309C + 0x14;   /* .rot */
D_800D30B8 = D_800D309C + 0x1C;   /* .range */
D_800D30BC = D_800D309C + 0x20;   /* .matrix */
D_800D30E4 = D_800D309C + 0x48;   /* .start */
D_800D3611 = D_800D3600 + 0x11;   /* image animation .dirty */
D_800D39B8 = D_800D36C8 + 0x2F0;  /* message [7].rect: the battle message image */

/* The resident's mode table (8001808c, mode 2) clears 800c3a70-800d39f4 for
 * this overlay; nothing in the image addresses the end of its last
 * variable, so the link checks it. */
ASSERT(battle_BSS_START == 0x800C3A70 && battle_BSS_END == 0x800D39F4,
       "the battle .bss differs from the resident mode table's bounds")
