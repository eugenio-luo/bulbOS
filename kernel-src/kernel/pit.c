#include <stdint.h>
#include <stdio.h>

#include <kernel/pic.h>
#include <kernel/pit.h>
#include <kernel/apic.h>

void
pit_change_modreg(uint8_t modreg)
{
        outb(modreg, PIT_MODREG);
}

uint16_t
pit_readcount(void)
{
        asm volatile ("cli");

        uint16_t count = inb(PIT_CHANNEL_0);
        count |= inb(PIT_CHANNEL_0) << 8;

        asm volatile ("sti");

        return count;
}

void
pit_setcount(uint16_t count)
{
        asm volatile ("cli");

        outb(count & 0xFF, PIT_CHANNEL_0);
        //outb((count & 0xFF00) >> 8, PIT_CHANNEL_0);

        asm volatile ("sti");
}

void
pit_init(void)
{
        uint8_t pit_modreg = 0;

        // 6th and 7th bit are clear because it uses channel 0

        pit_modreg |= (1 << 5);

        pit_modreg |= (1 << 1);
        // 1st, 2nd, 3rd bit are clear because by default we uses interrupts
        // 0th bit is clear we uses 16-bit binary

        pit_change_modreg(pit_modreg);
        pit_setcount(1);

        ioapic_legacy_irq_activate(0);
        
        kprintf("PIT setup SUCCESS\n");
}

void
pit_on(void)
{
        pic_clearmask(0);
}

void
pit_off(void)
{
        pic_setmask(0);
}
