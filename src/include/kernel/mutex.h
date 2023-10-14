#ifndef _KERNEL_MUTEX_H
#define _KERNEL_MUTEX_H

#include <stdint.h>

#include <kernel/task.h>

struct semaphore {
        uint32_t max_count;
        uint32_t current_count;
        struct task_info *waiting_tasks_start;
        struct task_info *waiting_tasks_end;
};

typedef struct condvar {
        void *condition;
        struct task_info *tasks;
        struct condvar *next;
} condvar_t;
 
typedef struct semaphore semaphore_t;

semaphore_t *semaphore_create(uint32_t max_count);
semaphore_t *mutex_create(void);
void semaphore_acquire(semaphore_t *semaphore);
void mutex_acquire(semaphore_t *mutex);
void semaphore_release(semaphore_t *semaphore);
void mutex_release(semaphore_t *mutex);

condvar_t *condvar_alloc(void *condition);
void condvar_wait(void *condition, semaphore_t *mutex);
void condvar_signal(void *condition);
void condvar_free(void *condition);

#endif
