#include <stdint.h>
#include <stdio.h>

#include <kernel/idt.h>
#include <kernel/apic.h>
#include <kernel/pic.h>
#include <kernel/isrs.h>
#include <kernel/keyboard.h>
#include <kernel/cmos.h>

static uint32_t spurious_case = 0;

/*
__attribute__ ((interrupt))
static void
_irq0(struct interrupt_frame *frame)
{
        (void) frame;
        // TODO: better way to handle this
        static int times = 0;
        
        kprintf("lapic: %d\n", ++times);

        lapic_sendEOI();
}

__attribute__ ((interrupt))
static void
_irq1(struct interrupt_frame *frame)
{
        (void) frame;
        keyboard_handlecode();
        pic_sendEOI(1);
}

__attribute__ ((interrupt))
static void
_irq2(struct interrupt_frame *frame)
{
        (void) frame;
        pic_sendEOI(2);
}

__attribute__ ((interrupt))
static void
_irq7(struct interrupt_frame *frame)
{
        (void) frame;
        uint16_t isr = pic_getisr(); 

        // NOT spurious IRQ case
        if (isr & (1 << 7)) {
                pic_sendEOI(7);
        } else {
                ++spurious_case;
        }
}

__attribute__ ((interrupt))
static void
_irq8(struct interrupt_frame *frame)
{
        (void) frame;
        cmos_interrupt_handler();
        
        pic_sendEOI(8);
}

__attribute__ ((interrupt))
static void
_irq15(struct interrupt_frame *frame)
{
        (void) frame;

        uint16_t isr = pic_getisr(); 

        // NOT spurious IRQ case
        if (isr & (1 << 15)) {
                pic_sendEOI(15);
        } else {
                ++spurious_case;
        }
}
*/

void
irq_init(void)
{
        kprintf("IRQ setup SUCCESS\n");
}

uint32_t
irq_get_spurious_case(void)
{
        return spurious_case;
}
