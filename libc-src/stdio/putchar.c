#include <stdint.h>
#include <stdio.h>

#if defined(__is_libk)
#include <kernel/tty.h>
#include <kernel/serialport.h>
#endif

static int32_t actualmode = TERMINAL_OUT;

void
setoutput(int32_t mode)
{
        actualmode = mode;
}

int32_t
getoutput(void)
{
        return actualmode;
}
        
int putchar(int ic) {
#if defined(__is_libk)
	char c = (char) ic;
	switch (actualmode) {
        case TERMINAL_OUT: 
                terminal_write(&c, sizeof(c));
                break;

        case SERIALPORT_OUT:
                serial_write(&c, sizeof(c));
                break;
        }
#else
	// TODO: Implement stdio and the write system call.
#endif
	return ic;
}
