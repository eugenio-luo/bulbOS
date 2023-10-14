#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <kernel/idt.h>
#include <kernel/vmm.h>
#include <kernel/debug.h>

idt_entry_t *idt_entries;
idt_ptr_t *idt_ptr;

void
idt_create(idt_entry_t *descriptor, uintptr_t offset, idt_flag_t flags)
{
        descriptor->offset_1 = offset & 0xFFFF;
        descriptor->offset_2 = (offset >> 16) & 0xFFFF;

        /* kernel code segment */
        descriptor->segment_selector = 0x8;
        descriptor->flags = flags & 0xFF;

        DPRINTF("[IDT] descriptor %d assigned handler at %x\n",
                descriptor - idt_entries, offset);
}

void
idt_init(void)
{
        kprintf("[IDT] setup STARTING\n");

        size_t idt_size = sizeof(idt_entry_t) * IDT_ENTRIES; 
        idt_entries = kmalloc(idt_size);
        memset(idt_entries, 0, idt_size);

        idt_ptr = kmalloc(sizeof(idt_ptr_t));
        idt_ptr->limit = idt_size - 1;
        idt_ptr->base = (uintptr_t) idt_entries;

        idt_flush(idt_ptr);
        
        kprintf("[IDT] setup COMPLETE\n");
}
