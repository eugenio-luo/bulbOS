/* void gdt_flush(gdt_ptr_t *gdt_ptr) */
        .global gdt_flush
gdt_flush:
        mov     4(%esp), %eax
        lgdt    (%eax)
        ljmp    $0x08, $flush_end
flush_end:
        mov     $0x10, %ax
        mov     %ax, %ds
        mov     %ax, %es
        mov     %ax, %fs
        mov     %ax, %gs
        mov     %ax, %ss
        ret
        
        .global tss_flush
tss_flush:
        mov     $0x2B, %ax
        ltr     %ax
        ret

        .global idt_flush
idt_flush:
        mov     4(%esp), %eax
        lidt    (%eax)
        ret
