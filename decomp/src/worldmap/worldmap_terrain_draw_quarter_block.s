.include "decomp/src/worldmap/screen_bounds.s"

# Draw one terrain quarter block of 8x8 cells as textured triangles.
# a0 = the block's cell words, nine per row (the ninth is skipped), a1 =
# ordering table, a2 = the next free POLY_FT3, whose colour/code word is
# preset (the callers pass the frame's buffer + worldmap_terrain_packet_count packets).
# The scratchpad holds TerrainDrawScratch: 9x9 SVECTOR vertices from
# 0x000 (row stride 0x48), 64 CLUTs at 0x288 and texture pages at 0x308.
# The GTE holds the caller's rotation, translation, projection and depth
# cue. Cell word bits used here: 8-10 texture page, 11 CLUT bank, 13/14
# flip the cell's 16x16 texture tile horizontally/vertically, 15 picks the
# diagonal, 16-19/20-23 the tile column/row. Each cell becomes triangles
# (TL, TR, BL) and (TR, BR, BL), or with bit 15 set (TL, BR, BL) and
# (TR, BR, TL). A triangle is drawn when RTPT reports no FLAG error, it
# passes screen_bounds_test, max(SZ1, SZ2, SZ3) < 0xF00 and NCLIP > 0.
# It is linked at OT entry max >> 4 with CLUT (IR0 >> 7) of the bank,
# IR0 (the third vertex's depth cue) limited to 0xFFF. A FLAG error on the
# first triangle skips the cell's second one as well. The packet count
# worldmap_terrain_packet_count is checked before each cell, so the block stops once 0x7FE
# packets exist; it is stored back on exit.
# Saves s0..s5, s5 in the caller's a0 home slot; clobbers a0, a2, a3, v0,
# v1 and t0..t9. Handwritten: saves beyond its 24-byte frame, ori for
# small constants and trapping addi.

# Project the three loaded vertices; if the triangle is accepted, emit a
# POLY_FT3 at a2 with the packed u | v << 8 coordinates in registers \uv0,
# \uv1 and \uv2. A FLAG error branches to \flag_reject, any other
# rejection to \reject. t1 counts packets and t9 holds cell << 16.
.macro terrain_triangle flag_reject, reject, uv0, uv1, uv2
    rtpt
    cfc2    $t8, $31            # FLAG
    mfc2    $t2, $12            # SXY0
    bltz    $t8, \flag_reject   # bit 31: some result out of range
     andi   $t8, $t2, 0xFFFF
    mfc2    $t3, $13            # SXY1
    mfc2    $t4, $14            # SXY2
    screen_bounds_test \reject
    mfc2    $t5, $17            # SZ1
    mfc2    $t6, $18            # SZ2
    mfc2    $t7, $19            # SZ3
    slt     $t8, $t6, $t5
    bnez    $t8, .Lterrain_sz3_\@
     nop
    addu    $t5, $t6, $zero
.Lterrain_sz3_\@:
    slt     $t8, $t7, $t5
    bnez    $t8, .Lterrain_depth_\@
     nop
    addu    $t5, $t7, $zero
.Lterrain_depth_\@:
    sltiu   $t8, $t5, 0xF00     # also rejects a negative maximum
    beqz    $t8, \reject
     sra    $t5, $t5, 4
    nclip
    sll     $t5, $t5, 2
    mfc2    $t7, $24            # MAC0: NCLIP
    mfc2    $t6, $8             # IR0: depth cue
    blez    $t7, \reject
     addu   $t5, $a1, $t5       # OT entry
    sltiu   $t8, $t6, 0x1000
    bnez    $t8, .Lterrain_fog_\@
     srl    $t7, $t9, 23
    sra     $t6, $s5, 12        # 0xFFF, from the address mask
.Lterrain_fog_\@:
    andi    $t7, $t7, 0xE       # page (cell bits 8-10) * 2
    addu    $t7, $t7, $a3
    lhu     $t7, 0x308($t7)     # texture page
    srl     $t8, $t9, 21
    andi    $t8, $t8, 0x40      # cell bit 11: the second 32 CLUTs
    sra     $t6, $t6, 7
    sll     $t6, $t6, 1
    addu    $t6, $t6, $t8
    addu    $t6, $t6, $a3
    lhu     $t6, 0x288($t6)     # CLUT for this depth
    lw      $t8, 0($t5)         # previous OT entry
    sll     $t6, $t6, 16
    sll     $t7, $t7, 16
    or      $t6, $t6, \uv0
    or      $t7, $t7, \uv1
    addiu   $t1, $t1, 1
    sw      $t2, 8($a2)         # xy0
    sw      $t3, 16($a2)        # xy1
    sw      $t4, 24($a2)        # xy2
    sh      \uv2, 28($a2)
    and     $s4, $a2, $s5
    lui     $t2, 0x0700         # seven words after the tag
    or      $t2, $t2, $t8       # previous entry, not masked
    addiu   $a2, $a2, 32
    sw      $t6, -20($a2)       # uv0 and CLUT
    sw      $t7, -12($a2)       # uv1 and texture page
    sw      $t2, -32($a2)       # tag
    sw      $s4, 0($t5)         # the entry now starts with this packet
.endm

glabel worldmap_terrain_draw_quarter_block
    addiu   $sp, $sp, -24
    sw      $s0, 4($sp)
    sw      $s1, 8($sp)
    sw      $s2, 12($sp)
    sw      $s3, 16($sp)
    sw      $s4, 20($sp)
    sw      $s5, 24($sp)        # beyond the frame: the caller's a0 slot
    lui     $a3, 0x1F80         # scratchpad
    lui     $s5, 0x00FF
    ori     $s5, $s5, 0xFFFF    # DMA address mask
    addu    $t0, $a3, $zero     # top-left vertex of the cell
    lui     $t1, %hi(worldmap_terrain_packet_count)
    lw      $t1, %lo(worldmap_terrain_packet_count)($t1)
    ori     $v0, $zero, 8       # rows left
.Lterrain_row:
    ori     $v1, $zero, 8       # cells left in the row
.Lterrain_cell:
    lw      $t7, 0($a0)
    ori     $t6, $zero, 1
    ori     $t5, $zero, 2
    sll     $t9, $t7, 16        # bit 15 (the diagonal) becomes the sign
    srl     $t8, $t7, 12
    andi    $t8, $t8, 0xF0      # u = tile column * 16
    srl     $t4, $t7, 8
    andi    $t4, $t4, 0xF000    # v = tile row * 16, in the high byte
    or      $t8, $t8, $t4
    sra     $t7, $t7, 13
    andi    $t7, $t7, 3         # flips: 1 = u, 2 = v, 3 = both
    # s0..s3: the texture coordinates of TL, TR, BL and BR.
    bnez    $t7, .Lterrain_uv_flip_u
     addu   $s0, $t8, $zero
    addiu   $s1, $t8, 0xF
    addiu   $s2, $t8, 0xF00
    j       .Lterrain_uv_ready
     addiu  $s3, $t8, 0xF0F
.Lterrain_uv_flip_u:
    bne     $t7, $t6, .Lterrain_uv_flip_v
     addiu  $s0, $t8, 0xF
    addu    $s1, $t8, $zero
    addiu   $s2, $t8, 0xF0F
    j       .Lterrain_uv_ready
     addiu  $s3, $t8, 0xF00
.Lterrain_uv_flip_v:
    bne     $t7, $t5, .Lterrain_uv_flip_uv
     addiu  $s0, $t8, 0xF00
    addiu   $s1, $t8, 0xF0F
    addu    $s2, $t8, $zero
    j       .Lterrain_uv_ready
     addiu  $s3, $t8, 0xF
.Lterrain_uv_flip_uv:
    addiu   $s0, $t8, 0xF0F
    addiu   $s1, $t8, 0xF00
    addiu   $s2, $t8, 0xF
    addu    $s3, $t8, $zero
.Lterrain_uv_ready:
    sltiu   $t8, $t1, 0x7FE
    beqz    $t8, .Lterrain_done
     nop

    # First triangle: TL, TR or (bit 15) BR, BL.
    lwc2    $0, 0($t0)          # TL
    lwc2    $1, 4($t0)
    lwc2    $4, 0x48($t0)       # BL
    lwc2    $5, 0x4C($t0)
    bgez    $t9, .Lterrain_first_tr
     nop
    lwc2    $2, 0x50($t0)       # BR
    lwc2    $3, 0x54($t0)
    j       .Lterrain_first
     addu   $s4, $s3, $zero
.Lterrain_first_tr:
    lwc2    $2, 8($t0)          # TR
    lwc2    $3, 12($t0)
    addu    $s4, $s1, $zero
    nop
.Lterrain_first:
    terrain_triangle .Lterrain_next_cell, .Lterrain_second, $s0, $s4, $s2

.Lterrain_second:
    # Second triangle: TR, BR, BL or (bit 15) TL.
    lwc2    $0, 8($t0)          # TR
    lwc2    $1, 12($t0)
    lwc2    $2, 0x50($t0)       # BR
    lwc2    $3, 0x54($t0)
    bgez    $t9, .Lterrain_second_bl
     nop
    lwc2    $4, 0($t0)          # TL
    lwc2    $5, 4($t0)
    j       .Lterrain_second_project
     addu   $s4, $s0, $zero
.Lterrain_second_bl:
    lwc2    $4, 0x48($t0)       # BL
    lwc2    $5, 0x4C($t0)
    addu    $s4, $s2, $zero
    nop
.Lterrain_second_project:
    terrain_triangle .Lterrain_next_cell, .Lterrain_next_cell, $s1, $s3, $s4

.Lterrain_next_cell:
    addiu   $a0, $a0, 4
    addi    $v1, $v1, -1
    bnez    $v1, .Lterrain_cell
     addiu  $t0, $t0, 8
    addiu   $a0, $a0, 4         # skip the row's ninth cell word
    addi    $v0, $v0, -1
    bnez    $v0, .Lterrain_row
     addiu  $t0, $t0, 8         # and its ninth vertex
.Lterrain_done:
    lui     $at, %hi(worldmap_terrain_packet_count)
    sw      $t1, %lo(worldmap_terrain_packet_count)($at)
    lw      $s5, 24($sp)
    lw      $s4, 20($sp)
    lw      $s3, 16($sp)
    lw      $s2, 12($sp)
    lw      $s1, 8($sp)
    lw      $s0, 4($sp)
    addiu   $sp, $sp, 24
    jr      $ra
     nop
endlabel worldmap_terrain_draw_quarter_block
