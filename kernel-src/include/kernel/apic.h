#ifndef _KERNEL_APIC_H
#define _KERNEL_APIC_H

#include <stdint.h>

#include <kernel/rsdt.h>

typedef struct {
        std_header_t header;
        uint32_t     lapic_addr;
        uint32_t     flags;
} __attribute__ ((packed)) madt_header_t;

typedef struct {
        uint8_t type;
        uint8_t length;
} madt_record_t;

typedef struct {
        madt_record_t header;
        uint8_t       processor_id;
        uint8_t       apic_id;
        uint32_t      flags;
} __attribute__ ((packed)) lapic_record_t;

typedef struct {
        madt_record_t header;
        uint8_t       ioapic_id;
        uint8_t       reserved;
        uint32_t      ioapic_addr;
        uint32_t      interrupt_base;       
} __attribute__ ((packed)) ioapic_record_t;

typedef struct {
        madt_record_t header;
        uint8_t       bus_source;
        uint8_t       irq_source;
        uint32_t      global_int;
        uint16_t      flags;
} __attribute__ ((packed)) int_over_record_t;

typedef enum {
        MADT_TYPE_LAPIC = 0,
        MADT_TYPE_IOAPIC = 1,
        MADT_TYPE_IOAPIC_OVERRIDE = 2,
        MADT_TYPE_IOAPIC_NMI = 3,
        MADT_TYPE_LAPIC_NMI = 4,
        MADT_TYPE_LAPIC_OVERRIDE = 5,
        MADT_TYPE_2XLAPIC = 9,
} madt_type_t;

#define MADT_TABLE_START         0x2C

#define LAPIC_TASK_PRIO_REG      0x80
#define LAPIC_EOI_REG            0xB0
#define LAPIC_SIV_REG            0xF0  /* spurious interrupt vector reg */
#define LAPIC_ERROR_STATUS_REG   0x280
#define LAPIC_TIMER_REG          0x320
#define LAPIC_LINT0_REG          0x350
#define LAPIC_LINT1_REG          0x360
#define LAPIC_ERROR_REG          0x370
#define LAPIC_INIT_COUNTER_REG   0x380
#define LAPIC_CURR_COUNTER_REG   0x390
#define LAPIC_TIMER_DIV_REG      0x3E0
#define LAPIC_SIZE               0x3F0

#define LAPIC_ENABLE             0x100
#define LAPIC_MASK               0x10000 

#define LAPIC_TIMER_MASK          (1 << 16)
#define LAPIC_TIMER_PERIODIC      (1 << 17)
#define LAPIC_TIMER_DIVISION_X1   0xB
#define LAPIC_TIMER_DIVISION_X2   0x0
#define LAPIC_TIMER_DIVISION_X16  0x3

#define IOAPIC_ID_REG            0x0
#define IOAPIC_VER_REG           0x1
#define IOAPIC_ARB_REG           0x2
#define IOAPIC_IRQ_REG           0x10
#define IOAPIC_SIZE              0x10

#define IOAPIC_MASK              (1 << 16) 

void lapic_sendEOI(void);
void ioapic_irq_activate(uint32_t apic_irq, uint32_t irq);
void ioapic_legacy_irq_activate(uint32_t irq);
void ioapic_irq_set_mask(uint32_t irq);
void ioapic_irq_clear_mask(uint32_t irq);
void apic_init(void);

#endif
