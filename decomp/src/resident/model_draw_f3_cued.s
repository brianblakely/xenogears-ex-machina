.include "decomp/src/resident/model_draw.s"

# Draw depth-cued triangles sorted by AVSZ3: model_draw_ft3_cued (POLY_FT3) is
# draw mode 4 of primitive types 1 and 5; the POLY_F3 entry model_draw_f3_cued
# is not listed in model_primitive_types. RGBC is loaded once with the model colour
# word model_color (see model_set_color). Faces are projected, culled,
# written, counted and linked as model_draw_gt3_avg's triangles. A linked
# face's colour word becomes DPCS of that colour, interpolated toward the
# far colour by the IR0 of the RTPT's last vertex, with the packet's own
# code byte less bit 0, so that a texture is modulated (not raw).
# a0 = records, a1 = count; t9/t8/a3 select the packet format as in
# model_draw_gt3_avg, whose shared exit this loop takes directly. Handwritten:
# no frame, two entry points, cross-routine exit, pipelined GTE work.
glabel model_draw_f3_cued
    model_packet 4, 4           # POLY_F3
    j       .Ldepth_cue_triangle_setup
     nop
alabel model_draw_ft3_cued  # 8002EF0C
    model_packet 8, 7           # POLY_FT3
.Ldepth_cue_triangle_setup:
    model_draw_save
    model_draw_state $t7
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    model_vertex0 $t0
    lwc2    $0, 0($t0)
    lwc2    $1, 4($t0)
    model_vertex1 $t0
    lwc2    $2, 0($t0)
    lwc2    $3, 4($t0)
    model_vertex2 $t0
    lwc2    $4, 0($t0)
    lwc2    $5, 4($t0)
    lui     $t0, %hi(model_color)
    lw      $t0, %lo(model_color)($t0)
    subu    $s3, $s3, $a3
    mtc2    $t0, $6             # RGBC
    lui     $s6, 0x00FF
    ori     $s6, $s6, 0xFFFF

.Ldepth_cue_triangle:
    rtpt
    beqz    $a1, .Lmodel_draw_exit
     addiu  $a1, $a1, -1
    addiu   $a0, $a0, 8
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    addu    $s3, $s3, $a3
    model_vertex0 $t0
    lwc2    $0, 0($t0)
    lwc2    $1, 4($t0)
    model_vertex1 $t0
    lwc2    $2, 0($t0)
    lwc2    $3, 4($t0)
    model_vertex2 $t0
    lwc2    $4, 0($t0)
    lwc2    $5, 4($t0)
    mfc2    $t0, $31            # LZCR (data register 31), not FLAG
    mfc2    $t1, $12
    bltz    $t0, .Ldepth_cue_triangle
     sltu   $t0, $t1, $v0
    mfc2    $t2, $13
    mfc2    $t3, $14
    nclip
    model_y_test 3, .Ldepth_cue_triangle, .Ldepth_cue_triangle_y
.Ldepth_cue_triangle_y:
     andi   $t0, $t1, 0xFFFF
    model_x_test 3, .Ldepth_cue_triangle, .Ldepth_cue_triangle_x
.Ldepth_cue_triangle_x:
     mfc2   $t4, $24            # NCLIP result
    avsz3
    blez    $t4, .Ldepth_cue_triangle
     and    $s3, $s3, $s6
    sw      $t1, 8($s3)
    addu    $t0, $s3, $t9
    sw      $t2, 8($t0)
    addu    $t0, $t0, $t9
    sw      $t3, 8($t0)
    mfc2    $t0, $7             # OTZ
    addiu   $s2, $s2, 1
    beqz    $t0, .Ldepth_cue_triangle
     dpcs
    srav    $t0, $t0, $t7
    sll     $t0, $t0, 2
    addu    $t0, $t0, $s4
    lbu     $t1, 7($s3)         # the packet's code
    mfc2    $t2, $22            # depth-cued RGB2
    sll     $t1, $t1, 24
    lui     $at, 0xFE00         # code without bit 0 (raw texture)
    and     $t1, $t1, $at
    and     $t2, $t2, $s6
    or      $t1, $t1, $t2
    sw      $t1, 4($s3)
    lw      $t1, 0($t0)
    sw      $s3, 0($t0)
    or      $t1, $t1, $t8
    j       .Ldepth_cue_triangle
     sw     $t1, 0($s3)
endlabel model_draw_f3_cued
