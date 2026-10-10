# Blend a0 RGB555 pixels from base (a3) towards target (the fifth argument,
# 16($sp)) by a1/32 into a2, through the GTE. Levels above 32 are clamped to
# 32 (a copy of target); there is no lower clamp. Each colour field becomes
# base + floor((target - base) * level / 32): GPF with sf=1 scales the
# signed in-place field differences by IR0 / 4096, and the result is masked
# back to its field before base is added. A field that decreases therefore
# carries into the bit above it (5, 10 or 15), which is ORed into the
# result. Bit 15 is otherwise clear.
# Clobbers a0, a1, a2, a3, v0, v1, t0..t7 and GTE IR0..IR3/MAC1..MAC3;
# no frame. The loop reads the halfword after the last target pixel.
# Handwritten: trapping add/addi and a dead copy of a2 in the clamp's
# delay slot.
glabel sprite_blend_pixels
    lw      $t0, 16($sp)        # target pixels
    slti    $v0, $a1, 33
    bnez    $v0, .Lblend_level
     addu   $t2, $a2, $zero     # unused: t2 is rewritten before any read
    addiu   $a1, $zero, 32
.Lblend_level:
    sll     $v0, $a1, 7
    mtc2    $v0, $8             # IR0: the level in 4.12 fixed point
    addiu   $a0, $a0, 1
.Lblend_pixel:
    addiu   $a0, $a0, -1
    beqz    $a0, .Lblend_done
     lhu    $t4, 0($t0)
    lhu     $v0, 0($a3)
    addi    $t0, $t0, 2
    andi    $t5, $v0, 0x1F      # base red
    andi    $t6, $v0, 0x3E0     # base green, unshifted
    andi    $t7, $v0, 0x7C00    # base blue, unshifted
    andi    $v1, $t4, 0x1F
    subu    $t3, $v1, $t5
    mtc2    $t3, $9             # IR1 = red difference
    andi    $v1, $t4, 0x3E0
    subu    $t2, $v1, $t6
    mtc2    $t2, $10            # IR2 = green difference
    andi    $v1, $t4, 0x7C00
    subu    $t1, $v1, $t7
    mtc2    $t1, $11            # IR3 = blue difference
    nop
    nop
    gpf     1
    mfc2    $t3, $9
    mfc2    $t2, $10
    mfc2    $t1, $11
    andi    $t3, $t3, 0x1F
    add     $t3, $t5, $t3
    andi    $t2, $t2, 0x3E0
    add     $t2, $t6, $t2
    andi    $t1, $t1, 0x7C00
    add     $t1, $t7, $t1
    or      $t3, $t3, $t2
    or      $t3, $t3, $t1
    sh      $t3, 0($a2)
    addi    $a3, $a3, 2
    j       .Lblend_pixel
     addi   $a2, $a2, 2
.Lblend_done:
    jr      $ra
     nop
endlabel sprite_blend_pixels
