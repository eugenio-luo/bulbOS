#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <kernel/page.h>
#include <kernel/idt.h>
#include <kernel/isrs.h>
#include <kernel/apic.h>

#define INT_HANDLER(NAME, TYPE, MESSAGE) \
        __attribute__ ((interrupt)) \
        static void \
        isr##NAME(interrupt_frame_t *frame) \
        { \
                TYPE##_handler(error_messages[MESSAGE], frame); \
                lapic_sendEOI(); \
        }

#define INT_CODE_HANDLER(NAME, TYPE, MESSAGE) \
        __attribute__ ((interrupt)) \
        static void \
        isr##NAME(interrupt_frame_t *frame, int32_t error_code) \
        { \
                code_##TYPE##_handler(error_messages[MESSAGE], frame, error_code); \
                lapic_sendEOI(); \
        }     

char *error_messages[] = {
        "divide-by-zero", // 0
        "debug", 
        "non-maskable interrupt",
        "breakpoint",
        "overflow",
        "bound range exceeded", // 5
        "invalid opcode", 
        "device not available",
        "double fault",
        "coprocessor segment overrun (LEGACY)",
        "invalid tss", // 10
        "segment not present",
        "stack-segment fault",
        "general protection fault",
        "page fault",
        "reserved, shouldn't never be called", // 15
        "x87 floating point exception",
        "alignment check",
        "machine check",
        "SIMD floating-point exception",
        "virtualization exception", // 20
        "control protection exception",
        "hypervisor injection exception",
        "VMM comunication exception",
        "Security exception", // 23
};

static void
trap_handler(char *message, interrupt_frame_t *frame)
{
        printf("TRAP: %s: at %x, %x\n", message, frame->cs, frame->eip);
        kprintf("TRAP: %s: at %x, %x\n", message, frame->cs, frame->eip);
}

static void
fault_handler(char *message, interrupt_frame_t *frame)
{
        printf("FAULT: %s: at %x, %x\n", message, frame->cs, frame->eip);
        kprintf("FAULT: %s: at %x, %x\n", message, frame->cs, frame->eip);
        abort();
}

static void
code_fault_handler(char *message, interrupt_frame_t *frame, int32_t error_code)
{
        printf("ERROR CODE: %x\n", error_code);
        kprintf("ERROR CODE: %x\n", error_code);
        fault_handler(message, frame);
}

static void
abort_handler(char *message, interrupt_frame_t *frame)
{
        printf("ABORT: %s, at %x, %x\n", message, frame->cs, frame->eip);
        kprintf("ABORT: %s, at %x, %x\n", message, frame->cs, frame->eip);
        abort();
}

static void
code_abort_handler(char *message, interrupt_frame_t *frame, int32_t error_code)
{
        printf("ERROR CODE: %x\n", error_code);
        kprintf("ERROR CODE: %x\n", error_code);
        abort_handler(message, frame);
}

INT_HANDLER(DE, fault, 0)
INT_HANDLER(DB, trap, 1)
INT_HANDLER(NMI, fault, 2)
INT_HANDLER(BP, trap, 3)
INT_HANDLER(OF, trap, 4)
INT_HANDLER(BR, fault, 5)
INT_HANDLER(UD, fault, 6)
INT_HANDLER(NM, fault, 7)
INT_CODE_HANDLER(DF, abort, 8)
INT_HANDLER(CSO, fault, 9)
INT_CODE_HANDLER(TS, fault, 10)
INT_CODE_HANDLER(NP, fault, 11)
INT_CODE_HANDLER(SS, fault, 12)        
INT_CODE_HANDLER(GP, fault, 13)
INT_HANDLER(RS, fault, 15)
INT_HANDLER(MF, fault, 16)
INT_CODE_HANDLER(AC, fault, 17)
INT_HANDLER(MC, abort, 18)
INT_HANDLER(XM, fault, 19)
INT_HANDLER(VE, fault, 20)
INT_CODE_HANDLER(CP, fault, 21)
INT_HANDLER(HV, fault, 22)
INT_CODE_HANDLER(VC, fault, 23)
INT_CODE_HANDLER(SX, fault, 24)

__attribute__ ((interrupt))
static void
isrPF(interrupt_frame_t *frame, int32_t error_code)
{
        uintptr_t addr;
        asm volatile ("mov %%cr2, %0\n\t" : "=r"(addr));
        kprintf("ADDR: %x\n", addr);
        code_fault_handler(error_messages[14], frame, error_code);
        lapic_sendEOI();
}

void
isrs_init(void)
{
        kprintf("[ISRS] setup STARTING\n");

        /* interrupt handler pointers */
        void (*int_handlers[])() = {
                isrDE, isrDB, isrNMI, isrBP, isrOF, isrBR, isrUD, /* 0 - 6 */
                isrNM, isrDF, isrCSO, isrTS, isrNP, isrSS, isrGP, /* 7 - 13 */
                isrPF, isrRS, isrMF,  isrAC, isrMC, isrXM, isrVE, /* 14 - 20 */
                isrCP, isrRS, isrRS,  isrRS, isrRS, isrRS, isrRS, /* 21 - 27 */
                isrHV, isrVC, isrSX,  isrRS,                      /* 28 - 31 */
        };

        /* static array + 0 avoids compiler warning */
        void (**func)() = int_handlers + 0;  
        idt_entry_t *entry = idt_entries;
        idt_entry_t *last = entry + N_OF_EXCEPTIONS;

        idt_flag_t flags = IDT_PRESENT | IDT_32B_INT;
        
        for (; entry < last; ++entry, ++func)
                idt_create(entry, (uintptr_t) *func, flags);
                
        kprintf("[ISRS] setup COMPLETE\n");
}
