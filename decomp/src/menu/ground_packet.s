# Emit one POLY_FT3 after the caller has projected and accepted its winding.
# Persistent state: a0 = OT, s4 = packet, s1 = packed UV high nibbles,
# s2 = four-corner UV template, s3 = packed tpage/clut, s5 = DMA address
# mask, s6 = high-halfword mask, s7 = seven-word DMA packet length.
.ifndef MENU_GROUND_PACKET_MACROS
.set MENU_GROUND_PACKET_MACROS, 1

.macro ground_triangle_packet second_uv, third_uv
    dpcs                        # depth-cue the caller's loaded RGBC
    lhu     $t0, 0($s2)
    lhu     $t1, \second_uv($s2)
    lhu     $t2, \third_uv($s2)
    and     $t3, $s3, $s6        # CLUT in UV0's upper halfword
    or      $t0, $s1, $t0
    or      $t0, $t0, $t3
    sw      $t0, 12($s4)
    sll     $t3, $s3, 16         # texture page in UV1's upper halfword
    or      $t1, $s1, $t1
    or      $t1, $t1, $t3
    sw      $t1, 20($s4)
    or      $t2, $s1, $t2
    sh      $t2, 28($s4)         # preserve the final UV padding halfword

    # Use max(SZ1, SZ2, SZ3), retaining the original signed comparisons.
    mfc2    $t0, $17
    mfc2    $t1, $18
    mfc2    $t2, $19
    slt     $t3, $t1, $t0
    bnez    $t3, .Lground_depth_y_\@
     swc2   $22, 4($s4)         # depth-cued RGB2 and packet command byte
    addu    $t0, $t1, $zero
.Lground_depth_y_\@:
    slt     $t3, $t2, $t0
    bnez    $t3, .Lground_depth_z_\@
     and    $s4, $s4, $s5       # continue packet writes via the DMA address
    addu    $t0, $t2, $zero
.Lground_depth_z_\@:
    srl     $t0, $t0, 4
    sll     $t0, $t0, 2
    addu    $t0, $t0, $a0
    lw      $t1, 0($t0)
    swc2    $14, 24($s4)
    swc2    $12, 8($s4)
    swc2    $13, 16($s4)
    sw      $s4, 0($t0)
    nop
    or      $t1, $t1, $s7       # old OT tag is not masked again
    addiu   $s4, $s4, 32
    sw      $t1, -32($s4)
.endm

.endif
