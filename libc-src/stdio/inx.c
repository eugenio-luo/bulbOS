#include <stdint.h>
#include <stdio.h>

inline uint8_t
inb(uint16_t port)
{
        uint8_t ret;
        asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port) );
        return ret;
}

inline uint32_t
inl(uint16_t port)
{
        uint32_t ret;
        asm volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
        return ret;
}

inline void
insl(uint16_t port, uint32_t *buffer, uint32_t count)
{
        for (uint32_t i = 0; i < count; ++i) {
                buffer[i] = inl(port);
        }
}
