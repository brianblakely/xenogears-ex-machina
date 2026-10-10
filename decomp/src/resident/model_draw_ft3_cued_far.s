.include "decomp/src/resident/model_draw.s"

# Draw depth-cued flat textured triangles (POLY_FT3) ordered by their
# farthest vertex: draw mode 5 of primitive types 1 and 5. As in
# model_draw_f3_cued, RGBC holds the model colour model_color and the colour
# word becomes its DPCS result with the packet's code less bit 0; DPCS is
# issued as soon as the y test passes. As in model_draw_gt3_far, the depth is
# the largest SZ (signed comparisons) shifted by model_ot_depth_shift + 2: the face
# is counted once front-facing and left unlinked at depth 0.
# a0 = records, a1 = count; takes model_draw_gt3_avg's shared exit directly.
# Handwritten: no frame, cross-routine exit, pipelined GTE work.
glabel model_draw_ft3_cued_far
    model_packet 8, 7           # POLY_FT3
    model_draw_save
    model_draw_state $t7
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    addi    $t7, $t7, 2
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

.Lfar_ft3:
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
    bltz    $t0, .Lfar_ft3
     sltu   $t0, $t1, $v0
    mfc2    $t2, $13
    mfc2    $t3, $14
    nclip
    model_y_test 3, .Lfar_ft3, .Lfar_ft3_y
.Lfar_ft3_y:
     dpcs
    andi    $t0, $t1, 0xFFFF
    model_x_test 3, .Lfar_ft3, .Lfar_ft3_x
.Lfar_ft3_x:
     mfc2   $t4, $24            # NCLIP result
    mfc2    $t5, $18            # SZ1..SZ3: the three vertices
    blez    $t4, .Lfar_ft3
     and    $s3, $s3, $s6
    sw      $t1, 8($s3)
    addu    $t0, $s3, $t9
    sw      $t2, 8($t0)
    mfc2    $t2, $19
    addu    $t0, $t0, $t9
    sw      $t3, 8($t0)
    mfc2    $t0, $17
    nop
    slt     $t3, $t5, $t0
    bnez    $t3, .Lfar_ft3_depth1
     lbu    $t1, 7($s3)         # the packet's code
    addu    $t0, $t5, $zero
.Lfar_ft3_depth1:
    slt     $t3, $t2, $t0
    bnez    $t3, .Lfar_ft3_depth2
     addiu  $s2, $s2, 1
    addu    $t0, $t2, $zero
.Lfar_ft3_depth2:
    mfc2    $t2, $22            # depth-cued RGB2
    beqz    $t0, .Lfar_ft3
     srav   $t0, $t0, $t7
    sll     $t0, $t0, 2
    addu    $t0, $t0, $s4
    sll     $t1, $t1, 24
    lui     $at, 0xFE00         # code without bit 0 (raw texture)
    and     $t1, $t1, $at
    and     $t2, $t2, $s6
    or      $t1, $t1, $t2
    sw      $t1, 4($s3)
    lw      $t1, 0($t0)
    sw      $s3, 0($t0)
    or      $t1, $t1, $t8
    j       .Lfar_ft3
     sw     $t1, 0($s3)
endlabel model_draw_ft3_cued_far
