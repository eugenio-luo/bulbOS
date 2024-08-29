#define PUSH_ALL \
        pushl %eax              ; \
        pushl %ebp              ; \
        pushl %edi              ; \
        pushl %esi              ; \
        pushl %edx              ; \
        pushl %ecx              ; \
        pushl %ebx

#define POP_ALL \
        popl  %ebx              ; \
        popl  %ecx              ; \
        popl  %edx              ; \
        popl  %esi              ; \
        popl  %edi              ; \
        popl  %ebp              ; \
        popl  %eax
        
        .global syscall_entry
syscall_entry:
        pushl %ecx
        pushl %edx
        pushl %ebp
        pushf
        movl  %esp, %ebp
        sysenter
        
syscall_exit:
        popf
        popl  %ebp
        popl  %edx
        popl  %ecx
        ret

        .global syscall_handler
        .extern syscall_table 
syscall_handler:
        pushl %ebp /* remember this is user space stack ptr */
        movl  (%ebp), %ebp

        PUSH_ALL

        movl  $syscall_table, %ebx
        call  *(%ebx, %eax, 4)
        movl  %eax, 0x18(%esp)

        POP_ALL

        popl  %ecx
        movl  $syscall_exit, %edx
        sti
        sysexit
