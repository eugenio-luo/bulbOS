#ifndef _STDIO_H
#define _STDIO_H 1

#include <sys/cdefs.h>
#include <stdint.h>
#include <stdarg.h>

#define EOF (-1)

#ifdef __cplusplus
extern "C" {
#endif

enum {
        TERMINAL_OUT,
        SERIALPORT_OUT,
};

void setoutput(int32_t mode);
int32_t getoutput(void);
int32_t vprintf(const char* __restrict, va_list);
int32_t printf(const char* __restrict, ...);
int32_t kprintf(const char* __restrict, ...);        
int putchar(int);
int puts(const char*);

// TODO: outb has arguments with wrong order 
void outb(uint8_t value, uint16_t port);
void outw(uint16_t value, uint16_t port);
void outl(uint32_t value, uint16_t port);
uint8_t inb(uint16_t port);
uint32_t inl(uint16_t port);
void insl(uint16_t port, uint32_t *buffer, uint32_t count);
void io_wait(void);
        
#ifdef __cplusplus
}
#endif

#endif
