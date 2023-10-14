#include <stdio.h>

inline void io_wait(void)
{
        outb(0, 0x80);
}
