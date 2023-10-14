#ifndef _KERNEL_IRQ_H
#define _KERNEL_IRQ_H

void irq_init(void);
uint32_t irq_get_spurious_case(void);

#endif
