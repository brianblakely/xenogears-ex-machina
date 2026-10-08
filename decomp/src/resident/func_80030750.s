.include "decomp/src/resident/model_draw.s"

# Draw environment-mapped flat textured triangles (POLY_FT3): every draw
# mode of primitive type 16, whose packets func_8002DAFC prepares as raw
# textured triangles on the override texture page and CLUT.
# Faces are projected and culled as in func_8002E010; the SXY words are
# stored around the NCLIP test, the first two also for a back face. A
# front-facing face with a nonzero AVSZ3 OTZ is counted (only then) and
# linked, and each vertex gets texture coordinates from its normal
# rotated into view space (MVMVA by the rotation matrix, no translation):
# u = ((x >> 6) + 0x40) & 0xFF and v = ((y >> 6) + 0x40) & 0xFF, mapping
# a unit normal (4.12) into 0..128. func_80030988 rewrites the shift
# counts and offsets of the labelled srl/addiu instructions in place,
# addressing them from D_800308D0.
# a0 = records, a1 = count. As in func_8002F4B4, a3 = normals - vertices
# and t9 holds the OT depth shift; s5..s7 hold the face's normal
# addresses, so a2 holds the DMA address mask. Exits through
# func_8002E010's shared exit with a3 = 32, the packet size.
# Handwritten: no frame, code patched at run time, trapping add/sub,
# cross-routine exit, pipelined GTE work.
glabel func_80030750
    model_draw_save
    model_draw_state $t9, normals=1
    lui     $a2, 0x00FF
    ori     $a2, $a2, 0xFFFF
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    model_vertex0 $t6
    model_vertex1 $t7
    model_vertex2 $t8
    lwc2    $2, 0($t7)
    lwc2    $3, 4($t7)
    lwc2    $4, 0($t8)
    lwc2    $5, 4($t8)
    addiu   $s3, $s3, -32
    sub     $a3, $a3, $s0       # normals - vertices

.Lenvmap:
    lwc2    $0, 0($t6)
    lwc2    $1, 4($t6)
    beqz    $a1, .Lenvmap_done
     rtpt
    add     $s5, $t6, $a3       # this face's normals
    add     $s6, $t7, $a3
    add     $s7, $t8, $a3
    addiu   $a1, $a1, -1
    addiu   $a0, $a0, 8
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
    addiu   $s3, $s3, 32
    model_vertex0 $t6
    model_vertex1 $t7
    model_vertex2 $t8
    lwc2    $2, 0($t7)
    lwc2    $3, 4($t7)
    lwc2    $4, 0($t8)
    lwc2    $5, 4($t8)
    mfc2    $t0, $31            # FLAG
    mfc2    $t1, $12
    bltz    $t0, .Lenvmap
     sltu   $t0, $t1, $v0
    mfc2    $t2, $13
    mfc2    $t3, $14
    nclip
    model_y_test 3, .Lenvmap, .Lenvmap_y
.Lenvmap_y:
     andi   $t0, $t1, 0xFFFF
    model_x_test 3, .Lenvmap, .Lenvmap_x
.Lenvmap_x:
     mfc2   $t0, $24            # NCLIP result
    sw      $t1, 8($s3)
    blez    $t0, .Lenvmap
     sw     $t2, 16($s3)
    avsz3
    sw      $t3, 24($s3)
    mfc2    $t3, $7             # OTZ
    and     $s3, $s3, $a2
    beqz    $t3, .Lenvmap
alabel D_800308D0               # func_80030988's patch base, a delay slot
     lwc2   $0, 0($s5)
    lwc2    $1, 4($s5)
    mvmva   1, 0, 0, 3, 0       # MAC = rotation * first normal
    addiu   $s2, $s2, 1
    srav    $t3, $t3, $t9
    sll     $t3, $t3, 2
    addu    $t3, $t3, $s4
    mfc2    $t0, $25            # view-space x and y
    mfc2    $t1, $26
    lwc2    $0, 0($s6)
    lwc2    $1, 4($s6)
    mvmva   1, 0, 0, 3, 0       # second normal
.Lenvmap_u0_shift:
    srl     $t0, $t0, 6
.Lenvmap_u0_offset:
    addiu   $t0, $t0, 0x40
    sb      $t0, 12($s3)        # u0
.Lenvmap_v0_shift:
    srl     $t1, $t1, 6
.Lenvmap_v0_offset:
    addiu   $t1, $t1, 0x40
    sb      $t1, 13($s3)        # v0
    mfc2    $t0, $25
    mfc2    $t1, $26
    lwc2    $0, 0($s7)
    lwc2    $1, 4($s7)
    mvmva   1, 0, 0, 3, 0       # third normal
.Lenvmap_u1_shift:
    srl     $t0, $t0, 6
.Lenvmap_u1_offset:
    addiu   $t0, $t0, 0x40
    sb      $t0, 20($s3)        # u1
.Lenvmap_v1_shift:
    srl     $t1, $t1, 6
.Lenvmap_v1_offset:
    addiu   $t1, $t1, 0x40
    sb      $t1, 21($s3)        # v1
    mfc2    $t0, $25
    mfc2    $t1, $26
.Lenvmap_u2_shift:
    srl     $t0, $t0, 6
.Lenvmap_u2_offset:
    addiu   $t0, $t0, 0x40
    sb      $t0, 28($s3)        # u2
.Lenvmap_v2_shift:
    srl     $t1, $t1, 6
.Lenvmap_v2_offset:
    addiu   $t1, $t1, 0x40
    sb      $t1, 29($s3)        # v2
    lw      $t1, 0($t3)
    sw      $s3, 0($t3)
    lui     $at, 0x0700         # POLY_FT3 tag length
    or      $t1, $t1, $at
    j       .Lenvmap
     sw     $t1, 0($s3)

.Lenvmap_done:
    ori     $a3, $zero, 32
    j       .Lmodel_draw_exit
     nop
endlabel func_80030750
