#ifndef _KERNEL_LOADER_H
#define _KERNEL_LOADER_H

#include <stddef.h>

typedef struct {
        uint8_t  id[16];
        uint16_t type;
        uint16_t machine;
        uint32_t version;
        uint32_t entry;
        uint32_t phoff;
        uint32_t shoff;
        uint16_t ehsize;
        uint16_t phentsize;
        uint16_t phnum;
        uint16_t shentsize;
        uint16_t shnum;
        uint16_t shstrndx;
} elf_header_t;

uintptr_t load_new_page_dir(void);

#endif
