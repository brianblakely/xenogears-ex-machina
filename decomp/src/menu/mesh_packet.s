# Shared assembly operations for the menu's handwritten flat mesh packets.
# These macros emit no code until used by the triangle/quad entry points.
.ifndef MENU_MESH_PACKET_MACROS
.set MENU_MESH_PACKET_MACROS, 1

# Persistent state: a2 = vertex work, a3 = primitive counter, t7 = packet,
# t8 = ordering-table slot, v0 = packed y limit, v1 = unsigned x limit.
# t9 is the DMA packet word count in the tag's high byte.
.macro mesh_packet_state words
    lui     $a2, %hi(D_8005953C)
    lw      $a2, %lo(D_8005953C)($a2)
    lui     $a3, %hi(D_80059578)
    lw      $a3, %lo(D_80059578)($a3)
    lui     $t7, %hi(D_80059424)
    lw      $t7, %lo(D_80059424)($t7)
    lui     $t8, %hi(D_80059568)
    lw      $t8, %lo(D_80059568)($t8)
    lui     $v0, %hi(D_800500FC)
    lw      $v0, %lo(D_800500FC)($v0)
    lui     $v1, %hi(D_800500F8)
    lw      $v1, %lo(D_800500F8)($v1)
    lui     $t9, \words*0x100
.endm

# a0 addresses four u16 vertex indices in an eight-byte face record.
# Each vertex work record is an eight-byte SVector. Quad code defers V0
# in t6 because it uses V0 again for the preceding quad's fourth point.
# Index 1's byte offset is masked with 0xFFF8, retaining only its low
# thirteen index bits. packet_bytes advances a slot after the record loads.
.macro mesh_face_vectors defer_first=0, packet_bytes=0
    lw      $t4, 0($a0)
    lhu     $t5, 4($a0)
.if \packet_bytes
    addiu   $t7, $t7, \packet_bytes
.endif
    andi    $t0, $t4, 0xFFFF
    sll     $t0, $t0, 3
.if \defer_first
    addu    $t6, $t0, $a2
.else
    addu    $t0, $t0, $a2
    lwc2    $0, 0($t0)
    lwc2    $1, 4($t0)
.endif
    srl     $t0, $t4, 13
    andi    $t0, $t0, 0xFFF8
    addu    $t0, $t0, $a2
    lwc2    $2, 0($t0)
    lwc2    $3, 4($t0)
    sll     $t0, $t5, 3
    addu    $t0, $t0, $a2
    lwc2    $4, 0($t0)
    lwc2    $5, 4($t0)
.endm

# Start one packet before the first slot and clear the segment/tag byte.
# Slots advance for culled faces too; existing colour/command words stay.
.macro mesh_packet_start bytes
    addiu   $t7, $t7, -\bytes
    lui     $at, 0x00FF
    ori     $at, $at, 0xFFFF
    and     $t7, $t7, $at
.endm

# Store packed SXY coordinates and prepend to the one supplied OT slot.
# The old OT word is ORed with the length without further masking.
.macro mesh_packet_link vertices, again
    sw      $t1, 8($t7)
    sw      $t2, 12($t7)
    sw      $t3, 16($t7)
.if \vertices == 4
    sw      $t4, 20($t7)
.endif
    lw      $t1, 0($t8)
    sw      $t7, 0($t8)
    or      $t1, $t1, $t9
    j       \again
     sw     $t1, 0($t7)
.endm

.macro mesh_packet_finish bytes
    addiu   $t7, $t7, \bytes
    lui     $at, %hi(D_80059578)
    sw      $a3, %lo(D_80059578)($at)
    lui     $at, %hi(D_80059424)
    sw      $t7, %lo(D_80059424)($at)
    jr      $ra
     nop
.endm
.endif
