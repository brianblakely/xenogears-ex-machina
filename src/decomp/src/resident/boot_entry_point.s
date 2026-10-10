# Program entry point (the executable header's initial pc). Zero the BSS:
# the words after heap_report_output, the final .data word, through boot_bss_last_word, the
# final word below the overlay area at 0x8006FAF0. The pointer advances
# before each store, so the end word itself is cleared.
# Then make the boot routine boot_main the return address and fall
# through into boot_reset_stack_and_gp, which installs the initial stack and gp and
# "returns" there. Only t0, t1 and ra are written; nothing is preserved.
# Handwritten: no frame, and it continues into the following routine
# instead of calling it.
glabel boot_entry_point
    lui     $t0, %hi(heap_report_output)
    addiu   $t0, $t0, %lo(heap_report_output)
    lui     $t1, %hi(boot_bss_last_word)
    addiu   $t1, $t1, %lo(boot_bss_last_word)
.Lentry_clear_bss:
    addiu   $t0, $t0, 4
    bne     $t0, $t1, .Lentry_clear_bss
     sw     $zero, 0($t0)
    lui     $ra, %hi(boot_main)
    addiu   $ra, $ra, %lo(boot_main)
    # No return: execution falls through into boot_reset_stack_and_gp.
endlabel boot_entry_point
