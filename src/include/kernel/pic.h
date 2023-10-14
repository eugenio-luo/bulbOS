#ifndef _KERNEL_PIC_H
#define _KERNEL_PIC_H

#include <stdint.h>

#define PIC_ICW1_ICW4     0x01
#define PIC_ICW1_INIT     0x10

#define PIC_ICW4_8086     0x01

#define PIC1         0x20
#define PIC2         0xA0
#define PIC1_COMMAND PIC1
#define PIC2_COMMAND PIC2
#define PIC1_DATA    (PIC1 + 1)
#define PIC2_DATA    (PIC2 + 1)
#define PIC_IRR      0x0A
#define PIC_ISR      0x0B

#define NEW_MAIN_OFFSET       0x20
#define NEW_SECONDARY_OFFSET  0x28

void pic_init(void);
void pic_sendEOI(uint8_t irq);
void pic_setmask(uint8_t index);
void pic_clearmask(uint8_t index);
     
#endif
