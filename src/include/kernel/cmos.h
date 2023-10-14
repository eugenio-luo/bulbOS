#ifndef _KERNEL_CMOS_H
#define _KERNEL_CMOS_H

#include <stdint.h>

typedef struct {
        uint8_t  second;
        uint8_t  minute;
        uint8_t  hour;
        uint8_t  day;
        uint8_t  month;
        uint32_t year;
        uint8_t  century;
} __attribute__ ((packed)) date_t;

void cmos_activate_interrupt(uint8_t interrupt);
void cmos_interrupt_handler(void);
void nmi_enable(void);
void nmi_disable(void);
void cmos_init(void);

#define CMOS_REG_PORT     0x70
#define CMOS_READ_PORT    0x71
#define CMOS_SECONDS_PORT 0x00
#define CMOS_MINUTES_PORT 0x02
#define CMOS_HOURS_PORT   0x04
#define CMOS_DAY_PORT     0x07
#define CMOS_MONTH_PORT   0x08
#define CMOS_YEAR_PORT    0x09
#define CMOS_CENTURY_PORT 0x32
#define CMOS_STATUSREG_A  0x0A
#define CMOS_STATUSREG_B  0x0B

#endif
