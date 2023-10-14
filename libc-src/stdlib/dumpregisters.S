        .global dumpregisters
        .extern dump_stackelement
dumpregisters:
        push    %ebp
        mov     %esp, %ebp

        pushal
        call    dump_stackelement

        mov     %ebp, %esp
        pop     %ebp
        ret
