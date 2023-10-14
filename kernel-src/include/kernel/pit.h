#ifndef _KERNEL_PIT_H
#define _KERNEL_PIT_H

#include <stdint.h>

void pit_change_modreg(uint8_t modreg);
uint16_t pit_readcount(void);
void pit_setcount(uint16_t count);
void pit_init(void);
void pit_on(void);
void pit_off(void);

#define PIT_CHANNEL_0 0x40
#define PIT_CHANNEL_1 0x41
#define PIT_CHANNEL_2 0x42
#define PIT_MODREG    0x43

#endif
