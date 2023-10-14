#ifndef _KERNEL_PIPE_H
#define _KERNEL_PIPE_H

#include <kernel/memory.h>
#include <kernel/mutex.h>
#include <kernel/file.h>

#define PIPE_SIZE   KIB(4)

typedef struct file file_t;

typedef struct pipe {
        struct semaphore *mutex;
        uint32_t size : 30;
        uint32_t type : 2;
        uint32_t b_read;
        uint32_t b_write;
        uint32_t read_open;
        uint32_t write_open;
        uint8_t *buffer;
} pipe_t;

typedef enum {
        PIPE_TYPE_UNBUFFERED = 0,
        PIPE_TYPE_BUF_BLOCK = 1,
        PIPE_TYPE_BUF_CHAR = 2,
} pipe_type_t;

/*
int pipe_write(pipe_t *pipe, uintptr_t addr, uint32_t size);
int pipe_read(pipe_t *pipe, uintptr_t addr, uint32_t size);
void pipe_close(pipe_t *pipe, int write);
*/
int pipe_alloc(file_t **f_read, file_t **f_write, uint32_t size, pipe_type_t type);

#endif
