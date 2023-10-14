#ifndef _KERNEL_RTC_H
#define _KERNEL_RTC_H

#include <stdint.h>

#include <kernel/cmos.h>

void rtc_setup_date(void);
void rtc_convert_date_to_time(void);
date_t* rtc_get_date(void);
uint32_t rtc_get_time_since_startup(void);
uint32_t rtc_get_epoch_time(void);
void rtc_update_time(void);

#endif
