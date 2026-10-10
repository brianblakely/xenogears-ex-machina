.include "decomp/src/menu/mesh_packet.s"

# Flat quads: a0 = eight-byte face records, a1 = face count. The first
# three points use RTPT; a front-facing quad's fourth point uses RTPS.
# t6 defers the next face's V0 load until the fourth point is finished.
# Every face increments the counter, including all culled faces.
# Bound tests accept any packed SXY below the y limit and any unsigned x
# below the x limit, independently. No depth/flag test or OT depth sort.
# The pipeline reads an extra face even when the requested count is zero.
# Handwritten: rtpt and lwc2 in branch delay slots (reorg never puts an asm
# there), and branch targets inside delay slots.
glabel arena_mesh_draw_flat_quads
    mesh_packet_state 5
    mesh_face_vectors 1
    mesh_packet_start 24
.Lmesh_quad_next:
    lwc2    $0, 0($t6)
    lwc2    $1, 4($t6)
    beqz    $a1, .Lmesh_quad_done
     rtpt
    addiu   $a1, $a1, -1
    addiu   $a0, $a0, 8
    mesh_face_vectors 1, 24
    mfc2    $t1, $12            # first three packed SXY results
    mfc2    $t2, $13
    mfc2    $t3, $14
    nclip

    # a0 already points at the next record: index 3 is at a0-2.
    lhu     $t0, -2($a0)
    nop
    sll     $t0, $t0, 3
    addu    $t0, $t0, $a2
    mfc2    $t4, $24            # NCLIP's MAC0
    addiu   $a3, $a3, 1
    blez    $t4, .Lmesh_quad_next
     lwc2   $0, 0($t0)
    lwc2    $1, 4($t0)
    rtps
    mfc2    $t4, $14            # fourth packed SXY

    sltu    $t0, $t1, $v0
    bnez    $t0, .Lmesh_quad_x
     sltu   $t0, $t2, $v0
    bnez    $t0, .Lmesh_quad_x
     sltu   $t0, $t3, $v0
    bnez    $t0, .Lmesh_quad_x
     sltu   $t0, $t4, $v0
    beqz    $t0, .Lmesh_quad_next
.Lmesh_quad_x:
     andi   $t0, $t1, 0xFFFF
    sltu    $t0, $t0, $v1
    bnez    $t0, .Lmesh_quad_emit
     andi   $t0, $t2, 0xFFFF
    sltu    $t0, $t0, $v1
    bnez    $t0, .Lmesh_quad_emit
     andi   $t0, $t3, 0xFFFF
    sltu    $t0, $t0, $v1
    bnez    $t0, .Lmesh_quad_emit
     andi   $t0, $t4, 0xFFFF
    sltu    $t0, $t0, $v1
    beqz    $t0, .Lmesh_quad_next
.Lmesh_quad_emit:
     nop
    mesh_packet_link 4, .Lmesh_quad_next
.Lmesh_quad_done:
    mesh_packet_finish 24
endlabel arena_mesh_draw_flat_quads
