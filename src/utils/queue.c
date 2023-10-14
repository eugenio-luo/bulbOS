#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <utils/queue.h>
#include <kernel/vmm.h>

queue_t*
queue_init(uint32_t capacity)
{
        queue_t *queue = kmalloc(sizeof(queue_t));
        queue->queue = kmalloc(sizeof(void*) * capacity);
        queue->capacity = capacity;
        queue->size = 0;
        queue->head = 0;
        
        return queue;
}

int
queue_push(queue_t *queue, void *value)
{
        if (queue->size + 1 > queue->capacity) {
                return -1;
        }

        size_t idx = (queue->head + queue->size) % queue->capacity;
        ++queue->size;
        queue->queue[idx] = value;
        return 0;
}

void
queue_pop(queue_t *queue)
{
        if (queue->size == 0)
                return;

        --queue->size;
        queue->head = (queue->head + 1) % queue->capacity;
}

void*
queue_top(queue_t *queue)
{
        return queue->queue[queue->head];
}
