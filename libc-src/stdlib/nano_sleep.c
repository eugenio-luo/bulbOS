#include <stdint.h>
#include <stdlib.h>

#include <kernel/task.h>
#include <kernel/hpet.h>

void
nano_sleep(uint64_t nanoseconds)
{
        nano_sleep_until(hpet_get_ns() + nanoseconds);
}

void
sleep(uint32_t milliseconds)
{
        nano_sleep(milliseconds * 1000000);
}
