.include "decomp/src/resident/model_draw.s"

# Draw Gouraud textured triangles (POLY_GT3) lit by their vertex normals:
# draw mode 1 of primitive type 3. Faces are projected and culled as in
# model_draw_gt3_avg; the SXY words are stored around the NCLIP test, the first
# two also for a back face. A front-facing face with a nonzero AVSZ3 OTZ
# is counted (only then), lit by NCT of its three vertex normals and
# linked. RGB0 is written with the packet's code byte; RGB1 and RGB2 are
# stored whole, their high byte (RGBC's code) landing in padding.
# a0 = records, a1 = count. In addition to the vertex array, the normal
# array model_current_normals is indexed by the same face indices: a3 = normals -
# vertices turns a vertex address into its normal's. t6..t8 carry the
# next face's vertex addresses to the loop top, so the current face's
# normal addresses are spilled to the scratchpad words 0x1F800000,
# 0x1F800004 and 0x1F800008 (a2 = base), and t9 holds the OT depth
# shift. Exits through model_draw_gt3_avg's shared exit with a3 = 40, the
# packet size. Handwritten: no frame, scratchpad spills, trapping
# add/sub, cross-routine exit, pipelined GTE work.
glabel model_draw_gt3_lit
    model_draw_save
    model_draw_state $t9, normals=1
    lui     $s6, 0x00FF
    ori     $s6, $s6, 0xFFFF
    lui     $a2, 0x1F80         # scratchpad
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    model_vertex0 $t6
    model_vertex1 $t7
    model_vertex2 $t8
    addiu   $s3, $s3, -40
    sub     $a3, $a3, $s0       # normals - vertices

.Llit_gt3:
    lwc2    $0, 0($t6)
    lwc2    $1, 4($t6)
    lwc2    $2, 0($t7)
    lwc2    $3, 4($t7)
    lwc2    $4, 0($t8)
    lwc2    $5, 4($t8)
    beqz    $a1, .Llit_gt3_done
     rtpt
    add     $t6, $t6, $a3       # this face's normals
    add     $t7, $t7, $a3
    sw      $t6, 0($a2)
    add     $t8, $t8, $a3
    sw      $t7, 4($a2)
    sw      $t8, 8($a2)
    addiu   $a1, $a1, -1
    addiu   $a0, $a0, 8
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    addiu   $s3, $s3, 40
    model_vertex0 $t6
    model_vertex1 $t7
    model_vertex2 $t8
    mfc2    $t0, $31            # LZCR (data register 31), not FLAG
    mfc2    $t1, $12
    bltz    $t0, .Llit_gt3
     sltu   $t0, $t1, $v0
    mfc2    $t2, $13
    mfc2    $t3, $14
    nclip
    model_y_test 3, .Llit_gt3, .Llit_gt3_y
.Llit_gt3_y:
     andi   $t0, $t1, 0xFFFF
    model_x_test 3, .Llit_gt3, .Llit_gt3_x
.Llit_gt3_x:
     mfc2   $t0, $24            # NCLIP result
    sw      $t1, 8($s3)
    blez    $t0, .Llit_gt3
     sw     $t2, 20($s3)
    avsz3
    sw      $t3, 32($s3)
    and     $s3, $s3, $s6
    mfc2    $t0, $7             # OTZ
    nop
    beqz    $t0, .Llit_gt3
     lw     $t1, 0($a2)
    addiu   $s2, $s2, 1
    lwc2    $0, 0($t1)          # V0..V2 = the vertex normals
    lw      $t2, 4($a2)
    lwc2    $1, 4($t1)
    lw      $t1, 8($a2)
    lwc2    $2, 0($t2)
    lwc2    $3, 4($t2)
    lwc2    $4, 0($t1)
    lwc2    $5, 4($t1)
    nct
    srav    $t0, $t0, $t9
    sll     $t0, $t0, 2
    addu    $t0, $t0, $s4
    lbu     $t1, 7($s3)         # the packet's code
    mfc2    $t2, $20            # RGB0
    sll     $t1, $t1, 24
    and     $t2, $t2, $s6
    or      $t1, $t1, $t2
    sw      $t1, 4($s3)
    swc2    $21, 16($s3)        # RGB1
    swc2    $22, 28($s3)        # RGB2
    lw      $t1, 0($t0)
    sw      $s3, 0($t0)
    lui     $at, 0x0900         # POLY_GT3 tag length
    or      $t1, $t1, $at
    j       .Llit_gt3
     sw     $t1, 0($s3)

.Llit_gt3_done:
    ori     $a3, $zero, 40
    j       .Lmodel_draw_exit
     nop
endlabel model_draw_gt3_lit
