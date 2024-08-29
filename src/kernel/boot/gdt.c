#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#include <kernel/gdt.h>
#include <kernel/tss.h>
#include <kernel/vmm.h>

gdt_entry_t *gdt_entries;
gdt_ptr_t *gdt_ptr;

static void
gdt_create(gdt_entry_t *descriptor, uint32_t base, uint32_t limit,
           uint8_t flags, gdt_access_t access)
{
        descriptor->limit = limit & 0xFFFF;
        descriptor->flags_limit = limit >> 16;
        
        descriptor->base_1 = base & 0xFFFF;
        descriptor->base_2 = (base >> 16) & 0xFF;
        descriptor->base_3 = (base >> 24) & 0xFF;

        descriptor->access = access;

        /* flag and limit share same byte so it is important to not
           delete precedently assigned limit */
        descriptor->flags_limit |= flags << 4;
}
        
void
gdt_init(void)
{
        kprintf("[GDT] setup STARTING\n");

        size_t gdt_size = sizeof(gdt_entry_t) * GDT_ENTRIES;

        gdt_entries = kmalloc(gdt_size);
        gdt_ptr = kmalloc(sizeof(gdt_ptr_t));
        
        gdt_ptr->size = gdt_size - 1;
        gdt_ptr->offset = (uintptr_t) gdt_entries;

        /* first empty entry, null descriptor */
        gdt_create(gdt_entries + 0, 0, 0, 0, 0);                

        gdt_flag_t flag = GDT_SIZE | GDT_GRAN; /* same flag for 4 of them */
        gdt_access_t access[] = {
                /* kernel mode code segment */
                GDT_RW | GDT_EXEC | GDT_TYPE | GDT_DPL0 | GDT_PRESENT,
                /* kernel mode data segment */
                GDT_RW |            GDT_TYPE | GDT_DPL0 | GDT_PRESENT,
                /* user mode code segment */
                GDT_RW | GDT_EXEC | GDT_TYPE | GDT_DPL3 | GDT_PRESENT,
                /* user mode data segment */
                GDT_RW |            GDT_TYPE | GDT_DPL3 | GDT_PRESENT,
        };

        for (int i = 1; i <= 4; ++i)
                gdt_create(gdt_entries + i, 0, 0xFFFFF, flag, access[i - 1]);

        /* task state segment */
        tss = kmalloc(sizeof(tss_t));
        gdt_access_t tss_access = GDT_ACCESS | GDT_EXEC | GDT_DPL0 | GDT_PRESENT;
        gdt_create(gdt_entries + 5, (uintptr_t) tss, sizeof(tss_t) - 1, 0, tss_access);
               
        gdt_flush(gdt_ptr);

        kprintf("[GDT] setup COMPLETE\n");
}
