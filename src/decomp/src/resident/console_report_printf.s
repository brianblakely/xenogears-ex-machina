# Report printf: continue in the console printf console_printf with every
# argument (registers and stack) untouched. The heap report hook
# heap_report_output is set back to this routine after a report to a host file.
# Handwritten: a tail jump, which GCC 2.x does not emit.
glabel console_report_printf
    j       console_printf
     nop
endlabel console_report_printf
