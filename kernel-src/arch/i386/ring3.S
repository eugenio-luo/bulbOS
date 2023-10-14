/* void enter_ring3(void) */
        .global enter_ring3
enter_ring3:
        cli
        mov $((4 * 8) | 3), %ax
        mov $0x23, %ax
        mov %ax, %ds
        mov %ax, %es
        mov %ax, %fs
        mov %ax, %gs

        movl %esp, %eax
        pushl $0x23
        pushl $((4 * 8) | 3)
        pushl %eax
        pushf 
        orl   $0x202, (%esp)
        pushl $((3 * 8) | 3)
        pushl $1f
        iret

1:
        ret
