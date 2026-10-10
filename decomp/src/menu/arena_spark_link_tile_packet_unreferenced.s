# GTE packet stub with an unresolved entry convention. Preserve its actual
# operations: build (view-origin - input) from low halfwords, load V0, then
# read the existing SXY2/SZ3 projection FIFO without issuing RTPS.
# a0 supplies the input halfwords AND the seed added to the stack-vector
# address to form the packet address. a1 is used as the OT base. The saved
# ra is never reloaded, and sp remains 0x28 bytes below its entry value.
# Do not present this entry as an ordinary C-callable projection helper.
# Handwritten: it returns with sp still lowered and ra never reloaded, and
# overwrites an offset it has just computed (dead code GCC deletes).
glabel arena_spark_link_tile_packet_unreferenced
    addiu   $sp, $sp, -0x28
    addu    $a2, $a0, $zero
    sw      $ra, 32($sp)

    # Three truncated differences; the vector's padding is left untouched.
    lui     $t0, %hi(arena_view_origin)
    lhu     $t0, %lo(arena_view_origin)($t0)
    lhu     $t1, 0($a0)
    nop
    subu    $t0, $t0, $t1
    sh      $t0, 16($sp)
    lui     $t0, %hi(arena_view_origin+4)
    lhu     $t0, %lo(arena_view_origin+4)($t0)
    lhu     $t1, 2($a0)
    nop
    subu    $t0, $t0, $t1
    sh      $t0, 18($sp)
    lui     $t0, %hi(arena_view_origin+8)
    lhu     $t0, %lo(arena_view_origin+8)($t0)
    lhu     $t1, 4($a0)
    nop
    subu    $t0, $t0, $t1
    sh      $t0, 20($sp)

    # The computed draw-buffer offset is overwritten by the stack address.
    lui     $v0, %hi(arena_draw_buffer_index)
    lbu     $v0, %lo(arena_draw_buffer_index)($v0)
    nop
    sll     $v0, $v0, 4
    addiu   $v0, $v0, 16
    addiu   $v0, $sp, 16
    addu    $a2, $a2, $v0
    lwc2    $0, 0($v0)
    lwc2    $1, 4($v0)
    nop
    nop
    addiu   $v0, $a2, 8
    swc2    $14, 0($v0)         # existing SXY2, not a newly projected vector
    addiu   $v0, $sp, 24
    mfc2    $t4, $19            # existing SZ3
    nop
    sra     $t4, $t4, 2
    sw      $t4, 0($v0)
    lw      $a0, 24($sp)
    nop
    sra     $a0, $a0, 2
    sll     $a0, $a0, 2
    addu    $a0, $a1, $a0
    addu    $a1, $a2, $zero
    lui     $at, 0x00FF
    ori     $at, $at, 0xFFFF
    and     $a1, $a1, $at
    lw      $t0, 0($a0)
    sw      $a1, 0($a0)
    lui     $at, 0x0300         # three-word DMA packet length
    or      $t0, $t0, $at
    sw      $t0, 0($a2)
    jr      $ra
     nop
endlabel arena_spark_link_tile_packet_unreferenced
