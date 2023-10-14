#include <stdio.h>
#include <stdint.h>

inline void
outb(uint8_t value, uint16_t port)
{
        asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

inline void
outw(uint16_t value, uint16_t port)
{
        asm volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

inline void
outl(uint32_t value, uint16_t port)
{
        asm volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}
