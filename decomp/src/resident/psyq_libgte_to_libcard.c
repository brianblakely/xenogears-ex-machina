/* 80048BC4: PsyQ SDK code following the libgte MSC00 data tag. Library assembly
 * remains an explicit SDK classification; these wrappers add no C coverage. */
#include "common.h"

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", InitGeom);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SquareRoot0);

/* 80048CDC */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_inverse_square_root);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", VectorNormalS);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", VectorNormal);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", VectorNormalSS);

/* 80048DD8 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_normalize_vector_in_registers);

/* 80048E94 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_orthonormalize_matrix);

/* 80048F7C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_weighted_sum_vector12);

/* 80048FCC */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_weighted_sum_vector0);

/* 8004901C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", LoadAverageShort12);

/* 800490A4 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_weighted_sum_svector0);

/* 8004912C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_weighted_sum_2_bytes);

/* 8004918C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_weighted_sum_3_bytes);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", MulMatrix0);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", CompMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", ApplyMatrixLV);

/* 800495DC */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_rotate_svector);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", PushMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", PopMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", ScaleMatrixL);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SetMulMatrix);

/* 8004998C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_rotate_vector);

/* 80049ACC */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_multiply_matrix_in_place);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", MulMatrix2);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", ApplyMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", ApplyMatrixSV);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", TransMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", ScaleMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SetRotMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SetLightMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SetColorMatrix);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SetTransMatrix);

/* 80049FAC */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_load_v0);

/* 80049FBC */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_load_v1);

/* 80049FCC */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_load_v2);

/* 80049FDC */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_load_v0_v1_v2);

/* 80049FFC */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_load_rgb_fifo);

/* 8004A010 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_set_ir1_ir2_ir3);

/* 8004A024 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_set_ir0);

/* 8004A030 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_set_sz1_sz2_sz3);

/* 8004A044 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_set_sz0_sz1_sz2_sz3);

/* 8004A05C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_set_sxy0_sxy1_sxy2);

/* 8004A070 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_set_rotation_diagonal);

/* 8004A084 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_set_mac1_mac2_mac3);

/* 8004A098 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_set_lzcs);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SetDQA);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SetDQB);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", ReadGeomOffset);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", ReadGeomScreen);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SetBackColor);

/* 8004A10C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SetFarColor);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SetGeomOffset);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SetGeomScreen);

/* 8004A15C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_apply_light_matrix);

/* 8004A180 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_depth_cue_color);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", NormalColor);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", NormalColor3);

/* 8004A1F4 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_normal_color_depth_cue);

/* 8004A218 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_normal_color_depth_cue3);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", NormalColorCol);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", NormalColorCol3);

/* 8004A2C4 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_color_depth_cue);

/* 8004A2EC */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_color_by_light_vector);

/* 8004A310 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_average_stored_z3);

/* 8004A320 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_average_stored_z4);

/* 8004A33C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_apply_color_matrix);

/* 8004A364 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_depth_cue_light_color);

/* 8004A38C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_depth_cue_color3);

/* 8004A3C8 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_interpolate_far_color);

/* 8004A3EC */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_square_vector12);

/* 8004A414 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", Square0);

/* 8004A43C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_average_z3);

/* 8004A45C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_average_z4);

/* 8004A480 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", OuterProduct12);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", OuterProduct0);

/* 8004A530 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_count_leading_zeros);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", RotTransSV);

/* 8004A57C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_square_svector12);

/* 8004A5B4 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_square_svector0);

/* 8004A5EC */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_square_svector12_to_vector);

/* 8004A61C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_square_svector0_to_vector);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", RotTransPers);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", RotTransPers3);

/* 8004A6DC */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", RotTrans);

/* 8004A70C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", NormalClip);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", RotTransPers4);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", RotAverage4);

/* 8004A83C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_project_front_quad);

/* 8004A8EC */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_transpose_matrix);

/* 8004A92C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", RotMatrixYXZ);

/* 8004ABBC */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", RotMatrix);

/* 8004AE4C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", RotMatrixX);

/* 8004AFEC */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", RotMatrixY);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", RotMatrixZ);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", ratan2);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _patch_gte);

/* 8004B514: the start of the words _patch_gte copies into the kernel, up to
 * VSync (the exception patch code below); a data label of its own, as mdec's
 * libpress_vlc_max_size, since the words follow _patch_gte's end. */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_patch_code);

/* 8004B51C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libgte_exception_patch_code);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", VSync);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", v_wait);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", ChangeClearRCnt);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", ResetCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", InterruptCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", DMACallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", VSyncCallback);

/* 8004B804 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libetc_set_vsync_callback_n);

/* 8004B834 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libetc_stop_interrupts);

/* 8004B864 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libetc_restart_interrupts);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", CheckCallback);

/* 8004B8A4 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libetc_get_interrupt_mask);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SetIntrMask);

/* 8004B8D8 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libetc_table_start_interrupts);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", trapIntr);

/* 8004BB9C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libetc_table_set_interrupt_callback);

/* 8004BCF0 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libetc_table_stop_interrupts);

/* 8004BD9C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libetc_table_restart_interrupts);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", memclr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", setjmp);

/* 8004BE8C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", longjmp);

/* 8004BED0: SDK object data tag in .text: the generated label is NOTYPE, size 8. */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libapi_object_tag_after_longjmp);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _96_remove);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", ReturnFromException);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", ResetEntryInt);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", HookEntryInt);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", startIntrVSync);

/* 8004C01C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libetc_vsync_clear_words);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", startIntrDMA);

/* 8004C2C4 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libetc_dma_clear_words);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SetVideoMode);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", GetVideoMode);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", PCopen);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", PCclose);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", PClseek);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", PCcreat);

/* 8004C38C */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", PCinit);

/* 8004C398 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", PCread);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _SN_read);

/* 8004C470 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", PCwrite);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _SN_write);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuInit);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _SpuInit);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuStart);

INCLUDE_RODATA(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libspu_timeout_format); /* 8001946C */

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _spu_init);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _spu_FwriteByIO);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _spu_Fr_);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _spu_t);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _spu_Fw);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _spu_Fr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _spu_FsetRXX);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _spu_FsetRXXa);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _spu_FsetDelayW);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _spu_FsetDelayR);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _spu_Fw1ts);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _SpuDataCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuQuit);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuInitMalloc);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuSetNoiseClock);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuSetReverb);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _SpuIsInAllocateArea_);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuReadDecodedData);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuSetIRQ);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuSetIRQCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _SpuCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuGetVoiceEnvelopeAttr);

/* 8004D818 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuRead);

/* 8004D878 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuWrite);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuSetTransferStartAddr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuSetTransferMode);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuSetTransferCallback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuSetCommonAttr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuSetReverbModeType);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _spu_setReverbAttr);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuClearReverbWorkArea);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", WaitEvent);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuSetReverbModeDepth);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuSetReverbModeDelayTime);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuSetReverbModeFeedback);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", SpuGetReverbModeType);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _card_info);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", InitCARD);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", StartCARD);

/* 8004E850 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libcard_bios_b4a_init_card);

/* 8004E860 */
INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", libcard_bios_b4b_start_card);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", StopCARD);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _patch_card);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _patch_card2);

INCLUDE_ASM(".local/decomp/resident/asm/nonmatchings/psyq_libgte_to_libcard", _ExitCard);
