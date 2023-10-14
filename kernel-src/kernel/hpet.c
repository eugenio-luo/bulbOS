#include <stdio.h>
#include <stdint.h>

#include <kernel/idt.h>
#include <kernel/hpet.h>
#include <kernel/rsdt.h>
#include <kernel/page.h>
#include <kernel/apic.h>
#include <kernel/isrs.h>
#include <kernel/vmm.h>

static uint32_t timers;
static uint32_t period_ns;
static uintptr_t hpet_addr;

static inline
uint64_t*
hpet_get_reg(hpet_reg_t reg)
{
        return (uint64_t*)(hpet_addr + reg);
}

static inline
uint64_t*
hpet_get_timer_reg(hpet_timer_reg_t reg, uint32_t timer)
{
        return (uint64_t*)(hpet_addr + HPET_TIMER_ADDR_OFFSET +
                           HPET_TIMER_OFFSET * timer + reg); 
}

static inline void
hpet_set(hpet_reg_t reg, hpet_flag_t flag)
{
        uint64_t *reg_ptr = hpet_get_reg(reg);
        *reg_ptr |= flag;
}

static inline void
hpet_timer_set(hpet_timer_reg_t reg, uint32_t timer, hpet_timer_flag_t flag)
{
        uint64_t *reg_ptr = hpet_get_timer_reg(reg, timer);
        *reg_ptr |= flag;
}

static inline void
hpet_clear(hpet_reg_t reg, hpet_flag_t bit)
{
        uint64_t *reg_ptr = hpet_get_reg(reg);
        *reg_ptr &= ~bit;
}

static inline void
hpet_timer_clear(hpet_timer_reg_t reg, uint32_t timer, hpet_timer_flag_t flag)
{
        uint64_t *reg_ptr = hpet_get_timer_reg(reg, timer);
        *reg_ptr &= ~flag;
}

void
hpet_enable(void)
{
        hpet_set(HPET_REG_CONFIG, HPET_ENABLE);
}

void
hpet_disable(void)
{
        hpet_clear(HPET_REG_CONFIG, HPET_ENABLE);
}

static void
hpet_parse(void)
{
        hpet_header_t *hpet_header = (hpet_header_t*) rsdt_get_entry("HPET");

        hpet_addr = (uintptr_t)hpet_header->address.address;
        page_identity_map(page_directory, hpet_addr, HPET_REGS_SIZE);

        timers = HPET_N_OF_TIMERS + 1;
        uintptr_t start = (uintptr_t) hpet_get_timer_reg(HPET_TIMER_REG_CAP, 0);
        uintptr_t end = (uintptr_t) hpet_get_timer_reg(HPET_TIMER_REG_CAP, timers);

        for (; start < end; start += HPET_TIMER_OFFSET)
                page_identity_map(page_directory, start, HPET_TIMER_SIZE);
}

static int32_t
hpet_enable_timer(uint32_t timer)
{
        if (timer >= timers) {
                kprintf("[HPET] timer %d doesn't exist\n", timer);
                return -1;
        }

        /* searching for allowed interrupt */
        uint32_t ioapic_irq = 0;
        for (; ~HPET_INT_MAP(timer) & (1 << ioapic_irq); ++ioapic_irq);
        
        /* set ioapic irq line */
        hpet_timer_set(HPET_TIMER_REG_CAP, timer, HPET_IRQ_ROUTING & ioapic_irq << 9);
        
        /* disable FSB interrupt mapping */
        hpet_timer_clear(HPET_TIMER_REG_CAP, timer, HPET_FSB_MAPPING);
        
        /* disable periodic interrupt */
        hpet_timer_clear(HPET_TIMER_REG_CAP, timer, HPET_PERIODIC);

        /* enable triggering of input */
        hpet_timer_set(HPET_TIMER_REG_CAP, timer, HPET_INT_ENABLED);

        /* enable edge trigger */
        hpet_timer_set(HPET_TIMER_REG_CAP, timer, HPET_LEVEL_TRIG);

        return ioapic_irq;
}

void 
hpet_set_comparator(uint32_t timer, uint64_t ns_100)
{
        uint64_t *timer_comp_reg = hpet_get_timer_reg(HPET_TIMER_REG_COMP, timer);
        uint64_t *counter_reg = hpet_get_reg(HPET_REG_COUNTER);

        uint64_t comparator_value = (ns_100 * 100) / period_ns;
        *timer_comp_reg = *counter_reg + comparator_value;
}

void
hpet_create_timer(uint32_t timer, uint32_t irq, void(*handler)(interrupt_frame_t*))
{
        uint32_t apic_irq = hpet_enable_timer(timer);

        idt_flag_t flag = IDT_PRESENT | IDT_32B_INT;
        idt_create(idt_entries + IRQ_OFFSET + LEGACY_PIC_OFFSET + irq,
                   (uintptr_t) handler, flag);

        /* ioapic will find the suitable irq */
        ioapic_irq_activate(apic_irq, irq);
        ioapic_irq_clear_mask(apic_irq);

        kprintf("[HPET] apic_irq: %x\n", apic_irq);
}

uint64_t
hpet_read_counter(void)
{
        return *hpet_get_reg(HPET_REG_COUNTER);
}

uint64_t
hpet_get_ns(void)
{
        return *hpet_get_reg(HPET_REG_COUNTER) * period_ns;
}

void
hpet_init(void)
{
        kprintf("[HPET] setup STARTING\n");

        hpet_parse();

        /* get period in nanoseconds */
        period_ns = HPET_PERIOD_FS / 1000000;
        kprintf("[HPET] period: %dns\n", period_ns, period_ns);

        /* disable legacy mapping */
        hpet_clear(HPET_REG_CONFIG, HPET_LEGACY_REPL);

        /* reset counter */
        uint64_t *counter = hpet_get_reg(HPET_REG_COUNTER);
        *counter = 0;
        
        hpet_enable();
        
        kprintf("[HPET] setup COMPLETE\n");
}

