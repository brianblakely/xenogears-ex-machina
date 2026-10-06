.include "decomp/src/menu/mesh_packet.s"

# Flat triangles: a0 = eight-byte face records, a1 = face count.
# RTPT is pipelined with the next face's input loads, including one extra
# readable face after the last. Count zero still loads/projects that face.
# Cull unless some packed SXY is below the y limit, NCLIP is positive,
# and some unsigned x is below the x limit. The two bound tests need not
# be satisfied by the same vertex. The counter advances after the first
# test, including backfaces and triangles rejected by the x test.
glabel func_8008C4B0
    mesh_packet_state 4
    mesh_face_vectors
    mesh_packet_start 20
.Lmesh_triangle_next:
    rtpt
    beqz    $a1, .Lmesh_triangle_done
     addiu  $a1, $a1, -1
    addiu   $a0, $a0, 8
    mesh_face_vectors 0, 20
    mfc2    $t1, $12            # SXY0
    mfc2    $t2, $13            # SXY1
    sltu    $t0, $t1, $v0
    mfc2    $t3, $14            # SXY2
    nclip
    bnez    $t0, .Lmesh_triangle_face
     sltu   $t0, $t2, $v0
    bnez    $t0, .Lmesh_triangle_face
     sltu   $t0, $t3, $v0
    beqz    $t0, .Lmesh_triangle_next
.Lmesh_triangle_face:
     mfc2   $t0, $24            # NCLIP result in MAC0, also branch delay
    addiu   $a3, $a3, 1
    blez    $t0, .Lmesh_triangle_next
     andi   $t0, $t1, 0xFFFF
    sltu    $t0, $t0, $v1
    bnez    $t0, .Lmesh_triangle_emit
     andi   $t0, $t2, 0xFFFF
    sltu    $t0, $t0, $v1
    bnez    $t0, .Lmesh_triangle_emit
     andi   $t0, $t3, 0xFFFF
    sltu    $t0, $t0, $v1
    beqz    $t0, .Lmesh_triangle_next
.Lmesh_triangle_emit:
     nop
    mesh_packet_link 3, .Lmesh_triangle_next
.Lmesh_triangle_done:
    mesh_packet_finish 20
endlabel func_8008C4B0
