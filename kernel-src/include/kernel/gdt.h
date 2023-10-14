#ifndef _KERNEL_GDT_H
#define _KERNEL_GDT_H

#include <stdint.h>

typedef enum {
        GDT_ACCESS =     (1 << 0),
        GDT_RW =         (1 << 1), /* readable bit/writable bit */
        GDT_DC =         (1 << 2), /* direction bit/conforming bit */
        GDT_EXEC =       (1 << 3),
        GDT_TYPE =       (1 << 4),

        GDT_DPL0 =       (0 << 5), /* cpu privilege level */
        GDT_DPL1 =       (1 << 5),
        GDT_DPL2 =       (2 << 5),
        GDT_DPL3 =       (3 << 5),
        
        GDT_PRESENT =    (1 << 7),
} gdt_access_t;

typedef enum {
        GDT_LONG_MODE =  (1 << 1),
        GDT_SIZE =       (1 << 2),
        GDT_GRAN =       (1 << 3),
} gdt_flag_t;

typedef struct {
        uint16_t limit;
        uint16_t base_1;
        uint8_t  base_2;
        uint8_t  access;
        uint8_t  flags_limit;
        uint8_t  base_3;
} __attribute__ ((packed)) gdt_entry_t;

typedef struct 
{
        uint16_t size;
        uint32_t offset;
} __attribute__ ((packed)) gdt_ptr_t;

#define GDT_ENTRIES    6

void gdt_init(void);
extern void gdt_flush(gdt_ptr_t *gdt_ptr);

#endif
