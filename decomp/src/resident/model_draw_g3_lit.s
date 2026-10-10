.include "decomp/src/resident/model_draw.s"

# Draw Gouraud triangles (POLY_G3) lit by their vertex normals from a
# cached colour: draw mode 1 of primitive type 2. model_prepare_g3_lit recorded
# one colour word per face at model_lit_color_cache; this consumes one per face,
# culled or not, and stores the advanced pointer (s7) at the end.
# Faces are projected and culled as in model_draw_gt3_avg; the SXY words are
# stored around the NCLIP test, the first two also for a back face. A
# front-facing face with a nonzero AVSZ3 OTZ is counted (only then), lit
# by NCCT (RGBC = the cached colour, V0..V2 = the vertex normals) and
# linked. RGB0 takes the cached colour's code byte; RGB1 and RGB2 are
# stored whole, their high byte landing in padding.
# a0 = records, a1 = count. As in model_draw_gt3_lit, a3 = normals - vertices,
# the face's normal addresses are spilled to the scratchpad words at
# 0x1F800000 (a2) and t9 holds the OT depth shift. Exits through
# model_draw_gt3_avg's shared exit with a3 = 28, the packet size.
# Handwritten: no frame, scratchpad spills, trapping add/sub,
# cross-routine exit, pipelined GTE work.
glabel model_draw_g3_lit
    model_draw_save
    model_draw_state $t9, normals=1
    lui     $s7, %hi(model_lit_color_cache)
    lw      $s7, %lo(model_lit_color_cache)($s7)
    lui     $s6, 0x00FF
    ori     $s6, $s6, 0xFFFF
    lui     $a2, 0x1F80         # scratchpad
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    model_vertex0 $t6
    model_vertex1 $t7
    model_vertex2 $t8
    addiu   $s3, $s3, -28
    sub     $a3, $a3, $s0       # normals - vertices

.Llit_g3:
    lwc2    $0, 0($t6)
    lwc2    $1, 4($t6)
    lwc2    $2, 0($t7)
    lwc2    $3, 4($t7)
    lwc2    $4, 0($t8)
    lwc2    $5, 4($t8)
    beqz    $a1, .Llit_g3_done
     rtpt
    add     $t6, $t6, $a3       # this face's normals
    add     $t7, $t7, $a3
    add     $t8, $t8, $a3
    sw      $t6, 0($a2)
    sw      $t7, 4($a2)
    sw      $t8, 8($a2)
    addiu   $a1, $a1, -1
    addiu   $a0, $a0, 8
    addiu   $s7, $s7, 4         # this face's colour ends here
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    addiu   $s3, $s3, 28
    model_vertex0 $t6
    model_vertex1 $t7
    model_vertex2 $t8
    mfc2    $t0, $31            # LZCR (data register 31), not FLAG
    mfc2    $t1, $12
    bltz    $t0, .Llit_g3
     sltu   $t0, $t1, $v0
    mfc2    $t2, $13
    mfc2    $t3, $14
    nclip
    model_y_test 3, .Llit_g3, .Llit_g3_y
.Llit_g3_y:
     andi   $t0, $t1, 0xFFFF
    model_x_test 3, .Llit_g3, .Llit_g3_x
.Llit_g3_x:
     mfc2   $t0, $24            # NCLIP result
    sw      $t1, 8($s3)
    blez    $t0, .Llit_g3
     sw     $t2, 16($s3)
    avsz3
    sw      $t3, 24($s3)
    and     $s3, $s3, $s6
    mfc2    $t0, $7             # OTZ
    nop
    beqz    $t0, .Llit_g3
     lw     $t2, 0($a2)
    addiu   $s2, $s2, 1
    lwc2    $0, 0($t2)          # V0..V2 = the vertex normals
    lw      $t1, 4($a2)
    lwc2    $1, 4($t2)
    lw      $t2, 8($a2)
    lwc2    $2, 0($t1)
    lwc2    $3, 4($t1)
    lw      $t1, -4($s7)        # cached colour
    lwc2    $4, 0($t2)
    lwc2    $5, 4($t2)
    mtc2    $t1, $6             # RGBC
    ncct
    srav    $t0, $t0, $t9
    sll     $t0, $t0, 2
    addu    $t0, $t0, $s4
    mfc2    $t2, $20            # RGB0
    lui     $at, 0xFF00         # the cached colour's code byte
    and     $t1, $t1, $at
    and     $t2, $t2, $s6
    or      $t1, $t1, $t2
    sw      $t1, 4($s3)
    swc2    $21, 12($s3)        # RGB1
    swc2    $22, 20($s3)        # RGB2
    lw      $t1, 0($t0)
    sw      $s3, 0($t0)
    lui     $at, 0x0600         # POLY_G3 tag length
    or      $t1, $t1, $at
    j       .Llit_g3
     sw     $t1, 0($s3)

.Llit_g3_done:
    lui     $at, %hi(model_lit_color_cache)
    sw      $s7, %lo(model_lit_color_cache)($at)
    ori     $a3, $zero, 28
    j       .Lmodel_draw_exit
     nop
endlabel model_draw_g3_lit
