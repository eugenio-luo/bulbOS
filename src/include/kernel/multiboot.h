#ifndef _KERNEL_MULTIBOOT_H
#define _KERNEL_MULTIBOOT_H

#include <stdint.h>

typedef struct {
        uint32_t tabsize;
        uint32_t strsize;
        uint32_t addr;
        uint32_t reserved;
} aout_sym_t;

typedef struct {
        uint32_t num;
        uint32_t size;
        uint32_t addr;
        uint32_t shndx;
} elf_header_t;

typedef enum {
        MULTIBOOT_INFO_MEMORY = 1 << 0,
        MULTIBOOT_INFO_MEM_MAP = 1 << 6,
        MULTIBOOT_INFO_VBE = 1 << 11,
        MULTIBOOT_INFO_FRAMEBUFFER = 1 << 12,
} multiboot_flags_t;

typedef struct {
        uint32_t flags;
        uint32_t mem_lower;
        uint32_t mem_upper;
        uint32_t boot_device;
        uint32_t cmdline;
        uint32_t mods_count;
        uint32_t mods_addr;
        union {
                aout_sym_t   aout_sym;
                elf_header_t elf_header;
        } u;
        uint32_t mmap_length;
        uint32_t mmap_addr;
        uint32_t drives_length;
        uint32_t drives_addr;
        uint32_t config_table;
        uint32_t bootloader_name;
        uint32_t apm_table;
        uint32_t vbe_control_info;
        uint32_t vbe_mode_info;
        uint16_t vbe_mode;
        uint16_t vbe_interface_seg;
        uint16_t vbe_interface_off;
        uint16_t vbe_interface_len;
        uint64_t framebuffer_addr;
        uint32_t framebuffer_pitch;
        uint32_t framebuffer_width;
        uint32_t framebuffer_height;
        uint8_t  framebuffer_bpp;
        uint8_t  framebuffer_color;
        uint8_t  color_info[5];
} __attribute__ ((packed)) multiboot_info_t;

typedef enum {
        MULTIBOOT_MEM_AVAILABLE = 1,
        MULTIBOOT_MEM_RESERVED = 2,
} mb_mmap_entry_type_t;

typedef struct {
        uint32_t size;
        uint32_t addr_low;
        uint32_t addr_high;
        uint32_t len_low;
        uint32_t len_high;
        uint32_t type;
} __attribute__ ((packed)) mb_mmap_entry_t;

#endif
