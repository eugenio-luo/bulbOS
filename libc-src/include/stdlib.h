#ifndef _STDLIB_H
#define _STDLIB_H 1

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((__noreturn__)) void abort(void);
void *malloc(size_t);
void nano_sleep(uint64_t nanoseconds);
void sleep(uint32_t milliseconds);
        
#ifdef __cplusplus
}
#endif

#endif
