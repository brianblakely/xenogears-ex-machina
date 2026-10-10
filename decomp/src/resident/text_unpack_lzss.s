# Decode LZSS-packed data: a0 = packed data, a1 = destination; returns the
# destination. The first word is the unpacked size. Each following flag
# byte describes the next eight tokens, least significant bit first. A clear
# bit copies one literal byte. A set bit reads a two-byte reference: its
# first byte and the low nibble of its second byte form a 12-bit distance
# back from the output position, and the high nibble plus 3 is the length
# (3..18 bytes). References copy forwards a byte at a time, so they may
# overlap their own output. The end is tested only before each flag byte,
# and only for equality: the stream must fill the output exactly at a group
# boundary, as tools/packed_container.py ensures.
# Leaf code in t registers: clobbers a0, a1, t0, t1, t3, t4, t6..t9.
# text_unpack_lzss_alloc falls through into this entry and shares its exit.
glabel text_unpack_lzss
    lw      $t7, 0($a0)         # unpacked size
    addiu   $a0, $a0, 4
    addu    $t7, $a1, $t7       # output end
    addu    $t6, $a1, $zero     # return value
    lbu     $t8, 0($a0)         # first flag byte
.Lunpack_group:
    beq     $a1, $t7, .Lunpack_return
     addiu  $a0, $a0, 1
    andi    $t1, $t8, 1
    ori     $t9, $zero, 8       # tokens left in the group
.Lunpack_token:
    lbu     $t0, 0($a0)         # literal, or the low byte of a distance
    srl     $t8, $t8, 1
    addiu   $t9, $t9, -1
    bnez    $t1, .Lunpack_reference
     addiu  $a0, $a0, 1
    sb      $t0, 0($a1)
    addiu   $a1, $a1, 1
    bnez    $t9, .Lunpack_token
     andi   $t1, $t8, 1
    j       .Lunpack_group
     lbu    $t8, 0($a0)         # next flag byte

.Lunpack_reference:
    lbu     $t4, 0($a0)
    addiu   $a0, $a0, 1
    andi    $t1, $t4, 0xF
    sll     $t1, $t1, 8
    or      $t0, $t0, $t1       # distance
    subu    $t1, $a1, $t0       # copy source
    srl     $t3, $t4, 4
    addiu   $t3, $t3, 3         # length
    addu    $t3, $t3, $t1       # copy source end
.Lunpack_copy:
    lb      $t0, 0($t1)
    addiu   $t1, $t1, 1
    sb      $t0, 0($a1)
    bne     $t1, $t3, .Lunpack_copy
     addiu  $a1, $a1, 1
    bnez    $t9, .Lunpack_token
     andi   $t1, $t8, 1
    j       .Lunpack_group
     lbu    $t8, 0($a0)         # next flag byte

.Lunpack_return:
    jr      $ra
     addu   $v0, $t6, $zero
endlabel text_unpack_lzss
