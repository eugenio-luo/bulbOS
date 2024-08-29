#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <kernel/apic.h>
#include <kernel/rsdt.h>
#include <kernel/pic.h>
#include <kernel/page.h>
#include <kernel/vmm.h>
#include <kernel/hpet.h>
#include <kernel/idt.h>
#include <kernel/task.h>

/* LAPIC timer is set up to interrupt every 10ms */
extern void delay_handler(interrupt_frame_t *frame);

static int processor_id;
static uintptr_t ioapic_addr = 0;
static uintptr_t lapic_addr = 0;
static uint32_t max_irqs;
static int32_t *ioapic_mapping;

static inline void
lapic_write_reg(uintptr_t reg, uint32_t data)
{
        *(volatile uint32_t*)(lapic_addr + reg) = data;
}

static inline uint32_t
lapic_read_reg(uintptr_t reg)
{
        return *(volatile uint32_t*)(lapic_addr + reg);
}

static inline void
ioapic_write_reg(uint32_t index, uint32_t reg)
{
        *(volatile uint32_t*)ioapic_addr = index;
        *(volatile uint32_t*)(ioapic_addr + 0x10) = reg;
}

static inline uint32_t
ioapic_read_reg(uint32_t index)
{
        *(volatile uint32_t*)ioapic_addr = index;
        return *(volatile uint32_t*)(ioapic_addr + 0x10);
}

static void
madt_parse(void)
{
        madt_header_t *madt_header = (madt_header_t*) rsdt_get_entry("APIC");
        lapic_addr = madt_header->lapic_addr;
        page_identity_map(page_directory, lapic_addr, LAPIC_SIZE); 

        /* traversing the entries */
        uintptr_t madt_entry = (uintptr_t)madt_header + sizeof(madt_header_t);
        uintptr_t madt_end = (uintptr_t)madt_header + madt_header->header.length;

        madt_record_t *madt_record;
        for (; madt_entry < madt_end; madt_entry += madt_record->length) {

                madt_record = (madt_record_t*) madt_entry;
                kprintf("[APIC] madt entry type %s: ",
                        (const char*[]){"LAPIC", "IOAPIC", "IOAPIC INT OVERRIDE",
                                "IOAPIC NMI", "LAPIC NMI", "LAPIC INT OVERRIDE",
                                "UNDEFINED", "UNDEFINED", "UNDEFINED", "2XAPIC"}
                        [madt_record->type]);
                
                switch (madt_record->type) {
                /* get processor id */
                case MADT_TYPE_LAPIC: {
                        lapic_record_t *lapic_record = (lapic_record_t*) madt_record;

                        kprintf("processor id: %x\n", lapic_record->processor_id);
                        
                        processor_id = lapic_record->processor_id;
                        break;
                }

                /* find the ioapic address and map it */
                case MADT_TYPE_IOAPIC: {
                        ioapic_record_t *ioapic_record = (ioapic_record_t*) madt_record;

                        kprintf("ioapic address: %x\n", ioapic_record->ioapic_addr);
                        
                        ioapic_addr = ioapic_record->ioapic_addr;
                        page_identity_map(page_directory, ioapic_addr, IOAPIC_SIZE);

                        max_irqs = (ioapic_read_reg(IOAPIC_VER_REG) >> 16) + 1;
                        ioapic_mapping = (int32_t*) kmalloc(sizeof(int32_t) * max_irqs);
                        memset(ioapic_mapping, -1, sizeof(int32_t) * max_irqs);                        
                        break;
                }

                /* map the overriden interrupt */
                case MADT_TYPE_IOAPIC_OVERRIDE: {
                        int_over_record_t *int_record = (int_over_record_t*) madt_record;

                        kprintf("int source: %d, global system int: %d\n",
                                int_record->irq_source, int_record->global_int);

                        ioapic_mapping[int_record->irq_source] = int_record->global_int;
                        break;
                }

                default:
                        kprintf("\n");
                        break;
                }
        }
}


static inline uint32_t
ioapic_get_irq(uint8_t irq, int is_low)
{
        return IOAPIC_IRQ_REG + 2 * irq + is_low;
}

void
lapic_sendEOI(void)
{
        lapic_write_reg(LAPIC_EOI_REG, 0);
}

static void
lapic_mask_int(void)
{
        lapic_write_reg(LAPIC_TIMER_REG, LAPIC_TIMER_MASK);
        lapic_write_reg(LAPIC_LINT0_REG, LAPIC_MASK);
        lapic_write_reg(LAPIC_LINT1_REG, LAPIC_MASK);
}

static void
lapic_init(void)
{
        kprintf("IOAPIC: %x\n", ioapic_addr);
        lapic_mask_int();

        asm volatile ("movl $0x1b, %ecx\n\t"
                      "rdmsr\n\t"
                      "bts  $11, %eax\n\t"
                      "wrmsr\n\t");
        
        lapic_write_reg(LAPIC_SIV_REG, 0xFF | LAPIC_ENABLE);
        lapic_write_reg(LAPIC_ERROR_REG, IRQ_OFFSET + LEGACY_PIC_OFFSET + 19);
        lapic_write_reg(LAPIC_ERROR_STATUS_REG, 0);
        lapic_write_reg(LAPIC_ERROR_STATUS_REG, 0);
        
        lapic_sendEOI();
        lapic_write_reg(LAPIC_TASK_PRIO_REG, 0);
}

static void
ioapic_init(void)
{
        // TODO: for now all ioapic irq are disactivated 
        for (uint32_t i = 0; i < max_irqs; ++i) {
                ioapic_write_reg(ioapic_get_irq(i, 0), IOAPIC_MASK);
                ioapic_write_reg(ioapic_get_irq(i, 1), 0);
        }
}

static uint32_t
ioapic_get_mapping(uint32_t irq)
{
        if (ioapic_mapping[irq] == -2) {
                printf("[APIC] irq got overwritten\n");
                abort();
        }
        
        if (irq >= max_irqs || ioapic_mapping[irq] == -1) {
                kprintf("WARNING: no mapping for legacy IRQ %d\n", irq);
                kprintf("other IRQ might have been overwritten\n");
                ioapic_mapping[irq] = -2;
                return irq;
        }

        return ioapic_mapping[irq];
}

void
ioapic_legacy_irq_activate(uint32_t irq)
{
        irq = ioapic_get_mapping(irq); 
        
        ioapic_write_reg(ioapic_get_irq(irq, 0), IRQ_OFFSET + irq);
        ioapic_write_reg(ioapic_get_irq(irq, 1), processor_id << 24);
}

void
ioapic_irq_activate(uint32_t apic_irq, uint32_t irq)
{
        ioapic_write_reg(ioapic_get_irq(apic_irq, 0), (1 << 16) | (IRQ_OFFSET + LEGACY_PIC_OFFSET + irq));
        ioapic_write_reg(ioapic_get_irq(apic_irq, 1), processor_id << 24);
}

void
ioapic_irq_set_mask(uint32_t irq)
{
        uint32_t reg = ioapic_read_reg(ioapic_get_irq(irq, 0));
        ioapic_write_reg(ioapic_get_irq(irq, 0), reg | (1 << 16));
}

void
ioapic_irq_clear_mask(uint32_t irq)
{
        uint32_t reg = ioapic_read_reg(ioapic_get_irq(irq, 0));
        ioapic_write_reg(ioapic_get_irq(irq, 0), reg & ~(1 << 16));
}

static volatile int done = 0;
extern __attribute__ ((interrupt)) void task_time_handler(interrupt_frame_t *frame);

static void
lapic_timer_init1(void)
{
        lapic_write_reg(LAPIC_TIMER_REG, IRQ_OFFSET + LEGACY_PIC_OFFSET + 0);
        lapic_write_reg(LAPIC_TIMER_DIV_REG, LAPIC_TIMER_DIVISION_X16);
        lapic_write_reg(LAPIC_INIT_COUNTER_REG, 0xFFFFFFFF);
        //hpet_set_comparator(0, 10000);
        hpet_set_comparator(0, 10000000);
        
        while (!done);
}

__attribute__ ((interrupt))
static void
lapic_timer_init2(interrupt_frame_t *frame)
{
        (void) frame;
        
        lapic_write_reg(LAPIC_TIMER_REG, LAPIC_TIMER_MASK);
        uint32_t ticks_1ms = 0xFFFFFFFF - lapic_read_reg(LAPIC_CURR_COUNTER_REG); 
        kprintf("ticks_1ms: %x\n", ticks_1ms);
        
        ioapic_irq_set_mask(2);
        lapic_sendEOI();
        idt_create(idt_entries + IRQ_OFFSET + LEGACY_PIC_OFFSET + 0,
                   (unsigned)task_time_handler, 0x8E);   
        ioapic_irq_clear_mask(0);
        done = 1;
        
        lapic_write_reg(LAPIC_INIT_COUNTER_REG, ticks_1ms);
        lapic_write_reg(LAPIC_TIMER_REG, (IRQ_OFFSET + LEGACY_PIC_OFFSET + 0) | LAPIC_TIMER_PERIODIC);
        lapic_write_reg(LAPIC_TIMER_DIV_REG, LAPIC_TIMER_DIVISION_X16);
}

void
apic_init(void)
{
        madt_parse();
       
        lapic_init();
        ioapic_init();

        hpet_create_timer(0, 1, &lapic_timer_init2); 
        lapic_timer_init1();
        
        kprintf("APIC setup SUCCESS\n");
}

