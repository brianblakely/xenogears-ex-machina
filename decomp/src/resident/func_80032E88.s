# Unpack into a new heap block: allocate the unpacked size (the packed
# data's first word) with func_80031BDC(size, a1), then fall through into
# the decoder func_80032EB4 with a0 = packed data, a1 = the block.
# a0 = packed data, a1 = allocation mode (passed on unchanged).
# Returns the block, or NULL without decoding when a quiet allocation fails
# (that path branches to the decoder's exit, which returns t6).
# Handwritten: ra and a0 are saved below sp before an 8-byte frame with no
# argument area, the frame is popped before the saves are read back, and
# the routine runs on into the next one.
glabel func_80032E88
    sw      $ra, -8($sp)
    sw      $a0, -4($sp)
    addiu   $sp, $sp, -8
    jal     func_80031BDC
     lw     $a0, 0($a0)         # unpacked size
    addiu   $sp, $sp, 8
    lw      $ra, -8($sp)
    addu    $t6, $v0, $zero     # the decoder returns t6
    beqz    $v0, .Lunpack_return
     lw     $a0, -4($sp)
    addu    $a1, $v0, $zero
    # Falls through into func_80032EB4.
endlabel func_80032E88
