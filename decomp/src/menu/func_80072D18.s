.include "decomp/src/menu/ground_packet.s"

# Draw selected height-map cells as pairs of textured triangles.
# a0 = OT, a1/a2 = x/z origin. Rotation, translation, depth cue and RGBC
# are already loaded in the GTE. Scratchpad holds right/left row limits
# at 0x000/0x080, four SVector corners at 0x100 and MapTable at 0x120.
# Rows 0..126 use the following row's heights. Each accepted span adds
# its cell count to model_submitted_primitive_count; emitted packets add to model_drawn_primitive_count.
# Return the emitted triangle count. No OT bounds or GTE flag test is made.
# Handwritten: callee saves below the unchanged sp without a frame, a
# trapping add and GTE stores (swc2) in branch delay slots.
glabel func_80072D18
    # This leaf saves registers below the caller's unchanged stack pointer.
    # Preserve its unused s0 save and the gap at sp-12 as well.
    sw      $s0, -4($sp)
    sw      $s1, -8($sp)
    sw      $s2, -16($sp)
    sw      $s3, -20($sp)
    sw      $s4, -24($sp)
    sw      $s5, -28($sp)
    sw      $s6, -32($sp)
    sw      $s7, -36($sp)
    lui     $s5, 0x00FF
    ori     $s5, $s5, 0xFFFF
    lui     $s6, 0xFFFF
    lui     $s7, 0x0700
    lui     $t8, 0x1F80
    addu    $t6, $t8, $zero
    addu    $t5, $t8, $zero
    addu    $a3, $t8, $zero
    ori     $t6, $t6, 0x80
    ori     $a3, $a3, 0x100
    ori     $t5, $t5, 0x120
    lui     $t0, %hi(D_800928A0)
    lbu     $t0, %lo(D_800928A0)($t0)
    lui     $t1, %hi(D_80092854)
    addiu   $t1, $t1, %lo(D_80092854)
    sll     $t0, $t0, 2
    add     $t0, $t0, $t1
    lw      $s4, 0($t0)
    addu    $t7, $zero, $zero    # row

.Lground_row:
    addu    $t3, $t7, $t8
    lbu     $t2, 0($t3)         # exclusive right column
    addu    $t3, $t7, $t6
    beqz    $t2, .Lground_next_row
     nop
    lbu     $t1, 0($t3)         # left column, 0xFF denotes an empty span
    ori     $t0, $zero, 0xFF
    beq     $t1, $t0, .Lground_next_row
     nop
    sltu    $t0, $t1, $t2
    beqz    $t0, .Lground_next_row
     subu   $s1, $t2, $t1
    beq     $t1, $t2, .Lground_next_row
     sll    $t0, $t7, 8
    subu    $v1, $t0, $a2       # row z, in 256-unit cells
    sll     $t0, $t1, 8
    subu    $v0, $t0, $a1       # first cell x
    lui     $t0, %hi(model_submitted_primitive_count)
    lw      $t0, %lo(model_submitted_primitive_count)($t0)
    nop
    addu    $t0, $t0, $s1
    lui     $at, %hi(model_submitted_primitive_count)
    sw      $t0, %lo(model_submitted_primitive_count)($at)
    sll     $t0, $t7, 7
    addu    $t0, $t0, $t1
    sll     $t0, $t0, 2
    lui     $t3, %hi(D_800928DC)
    lw      $t3, %lo(D_800928DC)($t3)
    sll     $s1, $s1, 2
    addu    $t4, $t3, $t0       # current four-byte height/texture record
    addu    $t9, $t4, $s1       # exclusive row end

.Lground_cell:
    # Corner order: upper left, lower right, upper right, lower left.
    sh      $v0, 24($a3)
    sh      $v0, 0($a3)
    sh      $v1, 20($a3)
    sh      $v1, 4($a3)
    addiu   $v0, $v0, 0x100
    sh      $v0, 16($a3)
    sh      $v0, 8($a3)
    addiu   $t0, $v1, 0x100
    sh      $t0, 28($a3)
    sh      $t0, 12($a3)
    lw      $t0, 0($t4)         # first height and this cell's texture flags
    lhu     $t1, 0x204($t4)
    lhu     $t2, 4($t4)
    lhu     $t3, 0x200($t4)
    sh      $t0, 2($a3)
    sh      $t1, 10($a3)
    sh      $t2, 18($a3)
    sh      $t3, 26($a3)
    lwc2    $0, 0($a3)
    lwc2    $1, 4($a3)
    lwc2    $2, 8($a3)
    lwc2    $3, 12($a3)
    lwc2    $4, 16($a3)
    lwc2    $5, 20($a3)
    srl     $t1, $t0, 16
    andi    $s1, $t1, 0xF0F0    # UV high nibbles
    rtpt
    andi    $t0, $t1, 3
    sll     $t0, $t0, 3
    addu    $s2, $t5, $t0       # one of four UV orientations
    andi    $t0, $t1, 12
    addu    $t0, $t0, $t5
    lw      $s3, 32($t0)        # one of four packed texture-page/CLUT pairs
    nclip
    lwc2    $2, 24($a3)         # preload lower left for the second triangle
    lwc2    $3, 28($a3)
    lwc2    $4, 8($a3)          # lower right
    mfc2    $t0, $24            # first NCLIP result
    lwc2    $5, 12($a3)
    blez    $t0, .Lground_second_triangle
     nop
    ground_triangle_packet 6, 2

.Lground_second_triangle:
    # V0 still holds the upper-left corner. The first packet's SXY values
    # were stored before this RTPT overwrites the projection FIFO.
    rtpt
    nclip
    mfc2    $t0, $24
    addiu   $t4, $t4, 4
    blez    $t0, .Lground_next_cell
     nop
    ground_triangle_packet 4, 6
.Lground_next_cell:
    bne     $t4, $t9, .Lground_cell
     nop
.Lground_next_row:
    addiu   $t7, $t7, 1
    slti    $t0, $t7, 0x7F
    bnez    $t0, .Lground_row
     nop

    # Count only emitted packets. Keep the pool's stored base unchanged.
    lui     $t0, %hi(D_800928A0)
    lbu     $t0, %lo(D_800928A0)($t0)
    lui     $t1, %hi(D_80092854)
    addiu   $t1, $t1, %lo(D_80092854)
    sll     $t0, $t0, 2
    addu    $t0, $t0, $t1
    lw      $t0, 0($t0)
    and     $s4, $s4, $s5
    and     $t0, $t0, $s5
    lui     $t1, %hi(model_drawn_primitive_count)
    lw      $t1, %lo(model_drawn_primitive_count)($t1)
    subu    $t0, $s4, $t0
    srl     $v0, $t0, 5
    addu    $t1, $t1, $v0
    lui     $at, %hi(model_drawn_primitive_count)
    sw      $t1, %lo(model_drawn_primitive_count)($at)
    lw      $s0, -4($sp)
    lw      $s1, -8($sp)
    lw      $s2, -16($sp)
    lw      $s3, -20($sp)
    lw      $s4, -24($sp)
    lw      $s5, -28($sp)
    lw      $s6, -32($sp)
    jr      $ra
     lw     $s7, -36($sp)
endlabel func_80072D18
    nop                         # original word before the vector helpers
