#ifndef _KERNEL_BIO_H
#define _KERNEL_BIO_H

#include <stdint.h>

#include <kernel/mutex.h>
#include <kernel/ide.h>

typedef struct bio_buf {
        int device;
        uint32_t block;
        int ref_count;
        int valid;
        int dirty;
        semaphore_t *mutex;
        uint32_t size;
        uint8_t *buffer;
        struct bio_buf *next;
        struct bio_buf *prev;
} bio_buf_t;

#define BIO_TABLE_SIZE     50
#define BIO_LOAD_FACTOR    75

void bio_init(void);
bio_buf_t* bio_read(int device, uint32_t sector, uint32_t size);
void bio_write(bio_buf_t *buf);
void bio_release(bio_buf_t *buf);
void bio_debug_print(void);

#endif
