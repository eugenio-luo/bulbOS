#include <stdint.h>
#include <stdio.h>

#include <kernel/pic.h>

void
pic_setmask(uint8_t index)
{
        uint16_t port;
        
        if (index < 8) {
                port = PIC1_DATA;
        } else {
                port = PIC2_DATA;
                index -= 8;
        }

        uint8_t value = inb(port) | (1 << index);
        outb(value, port);
}

void
pic_clearmask(uint8_t index)
{
        uint16_t port;

        if (index < 8) {
                port = PIC1_DATA;
        } else {
                port = PIC2_DATA;
                index -= 8;
        }

        uint8_t value = inb(port) & ~(1 << index);
        outb(value, port);
}

void
pic_init(void)
{
        outb(PIC_ICW1_INIT | PIC_ICW1_ICW4, PIC1_COMMAND);
        io_wait();
        outb(PIC_ICW1_INIT | PIC_ICW1_ICW4, PIC2_COMMAND);
        io_wait();

        outb(NEW_MAIN_OFFSET, PIC1_DATA);
        io_wait();
        outb(NEW_SECONDARY_OFFSET, PIC2_DATA);
        io_wait();

        outb(4, PIC1_DATA);
        io_wait();
        outb(2, PIC2_DATA);
        io_wait();

        outb(PIC_ICW4_8086, PIC1_DATA);
        io_wait();
        outb(PIC_ICW4_8086, PIC2_DATA);
        io_wait();

        outb(0xFF, PIC1_DATA);
        outb(0xFF, PIC2_DATA);
        
        kprintf("PIC setup SUCCESS, all lines closed\n");
}

void
pic_sendEOI(uint8_t irq)
{
        if (irq >= 8) {
                outb(0x20, PIC2_COMMAND);
        }

        outb(0x20, PIC1_COMMAND);
}
