#ifndef _KERNEL_VBE_H
#define _KERNEL_VBE_H

#include <stdint.h>

typedef struct {
        uint8_t signature[4];
        uint16_t version;
        uint32_t oem_string_ptr;
        uint32_t capabilities;
        uint32_t mode_ptr;
        uint16_t total_mem;
} __attribute__((packed)) vbe_info_t;

void vbe_init(void);

#endif
