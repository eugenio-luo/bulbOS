#include <string.h>
#include <stdio.h>

#include <kernel/tss.h>
#include <kernel/vmm.h>
#include <kernel/memory.h>

tss_t *tss;

void
tss_init(void)
{
        kprintf("[TSS] setup STARTING\n");

        memset(tss, 0, sizeof(tss_t));
        uint8_t *stack = kmalloc(PAGE_FRAME_SIZE);
        
        /* kernel data segment (third entry) */
        tss->ss0 = 0x10;
        tss->esp0 = (uintptr_t) stack + PAGE_LAST_DWORD;
        tss->iopb = sizeof(tss_t);

        tss_flush();
        
        kprintf("[TSS] setup COMPLETE\n");
}

        
