#include <stdio.h>

#include <kernel/cmos.h>
#include <kernel/rtc.h>

// TODO: complete rtc

#define SECONDS_IN_MINUTE 60
#define SECONDS_IN_HOUR   (SECONDS_IN_MINUTE * 60)
#define SECONDS_IN_DAY    (SECONDS_IN_HOUR * 24)
#define SECONDS_IN_YEAR   (SECONDS_IN_DAY * 365)

static date_t initial_date;
static uint32_t time_since_startup_doubled = 0; 
// for some reason epoch_time is behind some seconds
// this is a band-aid, TODO: fix this bad code
static uint32_t initial_epoch_time = 4;

static uint32_t
rtc_convert_month_to_days(uint8_t month)
{
        uint32_t days = 0;

        static uint16_t days_in_month[] = {
                31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
        };
        
        for (int i = 0; i < (month - 1); ++i) {
                days += days_in_month[i];
        }

        return days;
}

void
rtc_convert_date_to_time(void)
{
        // POSIX defines epoch time as such, it deliberately ignores leap seconds 

        initial_epoch_time += initial_date.second;
        initial_epoch_time += initial_date.minute * SECONDS_IN_MINUTE;
        initial_epoch_time += initial_date.hour * SECONDS_IN_HOUR;

        // minus 1 because the day hasn't concluded yet
        uint32_t tm_yday = initial_date.day + rtc_convert_month_to_days(initial_date.month) - 1;
        printf("%d\n", tm_yday);
        initial_epoch_time += tm_yday * SECONDS_IN_DAY;

        uint32_t tm_year = (initial_date.century * 100) + initial_date.year - 1900;
        initial_epoch_time += (tm_year - 70) * SECONDS_IN_YEAR; 

        // leap years
        initial_epoch_time += ((tm_year - 69) / 4) * SECONDS_IN_DAY;
        initial_epoch_time -= ((tm_year - 1) / 100) * SECONDS_IN_DAY;
        initial_epoch_time += ((tm_year + 299) / 400) * SECONDS_IN_DAY;
}

date_t*
rtc_get_date(void)
{
        return &initial_date;
}

uint32_t
rtc_get_time_since_startup(void)
{
        return time_since_startup_doubled / 2;
}

uint32_t
rtc_get_epoch_time(void)
{
        return initial_epoch_time + (time_since_startup_doubled / 2);
}

void
rtc_setup_date(void)
{
        cmos_activate_interrupt(0x10);
}

void
rtc_update_time(void)
{
        ++time_since_startup_doubled;
}
