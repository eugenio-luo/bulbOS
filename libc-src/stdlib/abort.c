#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern void dumpregisters(void);

__attribute__((__noreturn__))
void abort(void) {
#if defined(__is_libk)
	// TODO: Add proper kernel panic.
        printf("kernel: panic: abort()\n");
        kprintf("kernel: panic: abort()\n");

        // doesn't work still
        //dumpregisters();
        
        asm ("cli");
        while (1) asm ("hlt");
#else
	// TODO: Abnormally terminate the process as if by SIGABRT.
	printf("abort()\n");
#endif
	while (1) { }
	__builtin_unreachable();
}


void dump_stackelement(uint32_t edi, uint32_t esi, uint32_t ebp, uint32_t esp,
                       uint32_t ebx, uint32_t edx, uint32_t ecx, uint32_t eax)
{
        kprintf("EAX: %x\n", eax);
        kprintf("ECX: %x\n", ecx);
        kprintf("EDX: %x\n", edx);
        kprintf("EBX: %x\n", ebx);
        kprintf("ESP: %x\n", esp);
        kprintf("EBP: %x\n", ebp);
        kprintf("ESI: %x\n", esi);
        kprintf("EDI: %x\n", edi);
}
