#ifndef _KERNEL_IDT_H
#define _KERNEL_IDT_H

#include <stdint.h>

typedef struct 
{
        uint32_t eip;
        uint32_t cs;
        uint32_t eflags;
        uint32_t esp;
        uint32_t ss;
} __attribute__((packed)) interrupt_frame_t;

typedef enum {
        IDT_TASK     = 0x5,
        IDT_16B_INT  = 0x6,
        IDT_16B_TRAP = 0x7,
        IDT_32B_INT  = 0xE,
        IDT_32B_TRAP = 0xF,

        IDT_DPL0     = (0 << 5),
        IDT_DPL1     = (1 << 5),
        IDT_DPL2     = (2 << 5),
        IDT_DPL3     = (3 << 5),

        IDT_PRESENT  = (1 << 7),
} idt_flag_t;

typedef struct
{
        uint16_t offset_1;
        uint16_t segment_selector;
        uint8_t  reserved;
        uint8_t  flags;
        uint16_t offset_2;
} __attribute__((packed)) idt_entry_t;

typedef struct 
{
        uint16_t limit;
        uint32_t base;
} __attribute__((packed)) idt_ptr_t;

#define IDT_ENTRIES         256
#define N_OF_EXCEPTIONS     32
#define IRQ_OFFSET          N_OF_EXCEPTIONS
#define LEGACY_PIC_OFFSET   16

void idt_create(idt_entry_t *descriptor, uintptr_t offset, idt_flag_t flags);
void idt_init(void);
extern void idt_flush(idt_ptr_t *idt_ptr);
extern idt_entry_t *idt_entries;

#endif
