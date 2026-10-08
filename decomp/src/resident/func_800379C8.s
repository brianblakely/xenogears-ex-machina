# Report printf: continue in the console printf func_8003700C with every
# argument (registers and stack) untouched. The heap report hook
# D_800592B8 is set back to this routine after a report to a host file.
# Handwritten: a tail jump, which GCC 2.x does not emit.
glabel func_800379C8
    j       func_8003700C
     nop
endlabel func_800379C8
