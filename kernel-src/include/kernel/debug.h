#ifndef _KERNEL_DEBUG_H
#define _KERNEL_DEBUG_H

#include <stdio.h>

#ifdef DEBUG

#define DPRINTF(...) kprintf(__VA_ARGS__)

#define CLI() \
        asm volatile ("cli"); \
        kprintf("[INT] cleared interrupt\n");

#define STI() \
        asm volatile ("sti"); \
        kprintf("[INT] set interrupt\n");

#else

#define DPRINTF(...)

#define CLI() asm volatile ("cli");

#define STI() asm volatile ("sti");

#endif

#endif
