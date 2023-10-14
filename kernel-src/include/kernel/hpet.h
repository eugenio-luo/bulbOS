#ifndef _KERNEL_HPET_H
#define _KERNEL_HPET_H

#include <kernel/rsdt.h>
#include <kernel/idt.h>

typedef struct {
        uint8_t  address_space_id;
        uint8_t  register_bit_width;
        uint8_t  register_bit_offset;
        uint8_t  reserved;
        uint64_t address;
} __attribute__((packed)) hpet_address_struct_t;

typedef struct {
        std_header_t header; 
        uint8_t  hardware_rev_id;
        uint8_t  general_info;
        uint16_t pci_vendor_id;
        hpet_address_struct_t address;
        uint8_t  hpet_num;
        uint16_t min_tick;
        uint8_t  page_protection;
} __attribute__((packed)) hpet_header_t;

typedef enum {
        HPET_REG_CAPAB =      0x0,
        HPET_REG_CONFIG =     0x10,
        HPET_REG_INT_STAT =   0x20,
        HPET_REG_COUNTER =    0xF0,
} hpet_reg_t;

typedef enum {
        HPET_TIMER_REG_CAP =     0x0,
        HPET_TIMER_REG_COMP =    0x8,
        HPET_TIMER_REG_FSB =     0x10, 
} hpet_timer_reg_t;

#define HPET_REGS_SIZE           0xF7
#define HPET_TIMER_ADDR_OFFSET   0x100
#define HPET_TIMER_SIZE          0x18
#define HPET_TIMER_OFFSET        0x20
/* macros to access HPET info */
#define HPET_N_OF_TIMERS        (*hpet_get_reg(HPET_REG_CAPAB) >> 8 & 0xF) 
#define HPET_PERIOD_FS          (*hpet_get_reg(HPET_REG_CAPAB) >> 32)
#define HPET_INT_MAP(TIMER)     (*hpet_get_timer_reg(HPET_TIMER_REG_CAP, TIMER) >> 32)

typedef enum {
        /* general config reg */
        HPET_LEGACY_REPL =   (1 << 1),
        HPET_ENABLE =        (1 << 0),
} hpet_flag_t;

typedef enum {
        /* timer config reg */
        HPET_LEVEL_TRIG  =   (1 << 1),
        HPET_INT_ENABLED =   (1 << 2),
        HPET_PERIODIC    =   (1 << 3),
        HPET_IRQ_ROUTING =   (0x1F << 9),
        HPET_FSB_MAPPING =   (1 << 14),
} hpet_timer_flag_t;

void hpet_init(void);
void hpet_enable(void);
void hpet_disable(void);

/* 1 ns_100 = 100ns */
void hpet_set_comparator(uint32_t timer, uint64_t ns_100);
void hpet_create_timer(uint32_t timer, uint32_t irq, void(*func)(interrupt_frame_t*));
uint64_t hpet_read_counter(void);
uint64_t hpet_get_ns(void);

#endif
