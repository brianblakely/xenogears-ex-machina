# Darken a0 RGB555 pixels from a3 into a2 by a1/32 through the GTE.
# a0 = pixel count, a1 = level (32 and above copy the colours; there is
# no lower clamp), a2 = destination, a3 = source halfwords.
# IR0 = level * 128, and GPF with sf=1 multiplies each colour field, left
# in place, by IR0 / 4096 (rounding down). Bit 15 of the source is kept.
# A nonzero source pixel whose colour scales to zero becomes colour 1, so
# that it never turns into the transparent value 0; a zero pixel stays 0.
# Clobbers a0, a1, a2, a3, t2..t6 and GTE IR0..IR3/MAC1..MAC3; no frame.
# The loop reads the halfword after the last source pixel before it exits.
# Handwritten: trapping addi in the clamp and the `bne $zero, rt` operand
# order of its tests.
glabel func_80026F44
    slti    $t2, $a1, 32
    bne     $zero, $t2, .Ldarken_level
     nop
    addi    $a1, $zero, 32
.Ldarken_level:
    sll     $a1, $a1, 7
    mtc2    $a1, $8             # IR0: the level in 4.12 fixed point
    addiu   $t2, $zero, -1      # count sentinel
.Ldarken_pixel:
    addiu   $a0, $a0, -1
    beq     $a0, $t2, .Ldarken_done
     lhu    $t6, 0($a3)
    addiu   $a3, $a3, 2
    andi    $t5, $t6, 0x1F      # red
    andi    $t4, $t6, 0x3E0     # green, unshifted
    andi    $t3, $t6, 0x7C00    # blue, unshifted
    mtc2    $t5, $9             # IR1
    mtc2    $t4, $10            # IR2
    mtc2    $t3, $11            # IR3
    nop
    nop
    gpf     1
    mfc2    $t5, $9
    mfc2    $t4, $10
    mfc2    $t3, $11
    andi    $t5, $t5, 0x1F
    andi    $t4, $t4, 0x3E0
    andi    $t3, $t3, 0x7C00
    or      $t5, $t5, $t4
    or      $t5, $t3, $t5
    beq     $zero, $t6, .Ldarken_store
     nop
    bne     $zero, $t5, .Ldarken_store
     nop
    ori     $t5, $t5, 1         # keep a darkened pixel opaque
.Ldarken_store:
    andi    $t6, $t6, 0x8000
    or      $t6, $t6, $t5
    sh      $t6, 0($a2)
    addiu   $a2, $a2, 2
    j       .Ldarken_pixel
     nop
.Ldarken_done:
    jr      $ra
     nop
endlabel func_80026F44
