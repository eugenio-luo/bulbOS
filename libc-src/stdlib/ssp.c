#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#if UINT32_MAX == UINTPTR_MAX
#define STACK_CHK_GUARD 0xe2dee396
#else
#define STACK_CHK_GUARD 0x595e9fbd94fda766
#endif

// TODO: random canary value
// crashes OS if using rdrand
uintptr_t __stack_chk_guard = STACK_CHK_GUARD;  

__attribute__ ((noreturn))
void
__stack_chk_fail(void)
{
#if defined(__is_libk)
        printf("kernel: panic: stack smashing detected");
#else
        // TODO: implement stack check fail
#endif
        abort();
}
