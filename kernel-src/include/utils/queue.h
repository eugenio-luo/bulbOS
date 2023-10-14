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

queue_t* queue_init(uint32_t capacity);
int queue_push(queue_t *queue, void* value);
void queue_pop(queue_t *queue);
void* queue_top(queue_t *queue);

#endif
