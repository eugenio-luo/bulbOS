#include <stdint.h>
#include <stdio.h>

#include <kernel/cmos.h>
#include <kernel/pic.h>
#include <kernel/rtc.h>

#define DEFAULT_RTC_RATE 15

static uint8_t is_nmi_enabled = 0;
static uint8_t is_bin_mode    = 0;
static uint8_t is_24hour_mode = 0;

static inline uint8_t
cmos_convert_bcn(uint8_t input)
{
        return ((input / 16) * 10) + (input & 0xF);
}

static uint8_t
cmos_get_reg_value(uint8_t reg)
{
        outb((is_nmi_enabled << 7) | reg, CMOS_REG_PORT);
        io_wait();
        uint8_t ret = inb(CMOS_READ_PORT);

        return ret;
}

void
cmos_activate_interrupt(uint8_t interrupt)
{
        asm volatile ("cli");
        outb(0x8B, CMOS_REG_PORT);
        uint8_t statusreg_b = inb(CMOS_READ_PORT);
        outb(0x8B, CMOS_REG_PORT);
        outb(statusreg_b | interrupt, CMOS_READ_PORT);
        asm volatile ("sti");
}

static void
cmos_deactivate_interrupt(uint8_t interrupt)
{
        asm volatile ("cli");
        outb(0x8B, CMOS_REG_PORT);
        uint8_t statusreg_b = inb(CMOS_READ_PORT);
        outb(0x8B, CMOS_REG_PORT);
        outb(statusreg_b & ~interrupt, CMOS_READ_PORT);
        asm volatile ("sti");
}

void
cmos_handle_update_interrupt(void)
{
        date_t *initial_date = rtc_get_date();
        
        asm volatile ("cli");
        initial_date->second = cmos_get_reg_value(CMOS_SECONDS_PORT);
        initial_date->minute = cmos_get_reg_value(CMOS_MINUTES_PORT);
        initial_date->hour = cmos_get_reg_value(CMOS_HOURS_PORT);
        initial_date->day = cmos_get_reg_value(CMOS_DAY_PORT);
        initial_date->month = cmos_get_reg_value(CMOS_MONTH_PORT);
        initial_date->year = cmos_get_reg_value(CMOS_YEAR_PORT);
        initial_date->century = cmos_get_reg_value(CMOS_CENTURY_PORT);
        asm volatile ("sti");

        if (!is_24hour_mode) {
                uint8_t is_PM = initial_date->hour >> 7;
                
                initial_date->hour = is_PM * 12 + initial_date->hour; 
                if (initial_date->hour == 24) initial_date->hour = 0;
        }
        
        if (!is_bin_mode) {
                initial_date->second = cmos_convert_bcn(initial_date->second);
                initial_date->minute = cmos_convert_bcn(initial_date->minute);
                initial_date->hour = cmos_convert_bcn(initial_date->hour);
                initial_date->day = cmos_convert_bcn(initial_date->day);
                initial_date->month = cmos_convert_bcn(initial_date->month);
                initial_date->year = cmos_convert_bcn(initial_date->year); 
                initial_date->century = cmos_convert_bcn(initial_date->century);
        }

        rtc_convert_date_to_time();
        cmos_deactivate_interrupt(0x10);
        cmos_activate_interrupt(0x40);
}

void
nmi_enable(void)
{
        uint8_t cmos_reg = inb(CMOS_REG_PORT);
        cmos_reg &= 0x7F;
        
        outb(cmos_reg, CMOS_REG_PORT);
        io_wait();
        inb(CMOS_READ_PORT);
        
        is_nmi_enabled = 1;
}

void
nmi_disable(void)
{
        uint8_t cmos_reg = inb(CMOS_REG_PORT);
        cmos_reg |= 0x80;

        outb(cmos_reg, CMOS_REG_PORT);
        io_wait();
        inb(CMOS_READ_PORT);

        is_nmi_enabled = 0;
}

static void
cmos_check_statb_reg(void)
{
        uint8_t statusreg_b = cmos_get_reg_value(0x0B);
        
        is_24hour_mode = (statusreg_b >> 1) & 1;
        is_bin_mode = (statusreg_b >> 2) & 1;
}

static void
rtc_set_frequency(uint8_t rate)
{
        rate &= 0x0F;
        asm volatile ("cli");

        outb(0x0A, CMOS_REG_PORT);
        uint8_t statusreg_a = inb(CMOS_READ_PORT);
        outb(0x0A, CMOS_REG_PORT);
        outb((statusreg_a & 0xF0) | rate, CMOS_READ_PORT);

        asm volatile ("sti");
}

void
cmos_init(void)
{
        rtc_set_frequency(DEFAULT_RTC_RATE);
        nmi_enable();
        cmos_check_statb_reg();

        pic_clearmask(8);

        kprintf("CMOS setup SUCCESS\n");
}

void
cmos_interrupt_handler(void)
{
        outb(0x0C, CMOS_REG_PORT);
        uint8_t interrupt_type = inb(CMOS_READ_PORT);

        if ((interrupt_type >> 6) & 1) { 
                // periodic interrupt
                rtc_update_time();
                
        } else if ((interrupt_type >> 5) & 1) {
                // alarm interrupt
                
        } else {
                // update ended interrupt
                cmos_handle_update_interrupt();
        }
}
