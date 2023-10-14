#ifndef _STRING_H
#define _STRING_H 1

#include <sys/cdefs.h>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int memcmp(const void*, const void*, size_t);
int strcmp(const void*, const void*);
void* memcpy(void* __restrict, const void* __restrict, size_t);
void* memmove(void*, const void*, size_t);
void* memset(void*, int, size_t);
size_t strlen(const char*);
size_t itoa(int value, char *str, int base);
size_t uitoa(uint32_t value, char *str, uint32_t base);
    
#ifdef __cplusplus
}
#endif

#endif
