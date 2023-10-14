#ifndef _KERNEL_BQUEUE_H
#define _KERNEL_BQUEUE_H

#include <stdint.h>

typedef struct
{
        void **queue;
        uint32_t capacity;
        uint32_t size;
        uint32_t head;
} queue_t;

queue_t queue_init(uint8_t *base, uint32_t capacity);
int8_t queue_push(queue_t *queue, uint8_t value);
void queue_pop(queue_t *bqueue);
uint8_t bqueue_top(queue_t *bqueue);

#endif
