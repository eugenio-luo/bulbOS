#ifndef _KERNEL_VMM_H
#define _KERNEL_VMM_H

#include <stddef.h>

typedef struct node_t {
        union {
                size_t size;
                uintptr_t addr;
        };
        struct node_t *next;
} node_t; 

void vmm_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);
void vmm_print_kheap(void);

#endif
