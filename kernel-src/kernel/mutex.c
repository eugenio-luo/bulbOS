#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <kernel/mutex.h>
#include <kernel/task.h>
#include <kernel/vmm.h>

condvar_t *condvars;

semaphore_t*
semaphore_create(uint32_t max_count)
{
        semaphore_t *semaphore = kmalloc(sizeof(semaphore_t));
        if (!semaphore) return NULL;

        semaphore->max_count = max_count;
        semaphore->current_count = 0;
        semaphore->waiting_tasks_start = NULL;
        semaphore->waiting_tasks_end = NULL;

        return semaphore;
}

semaphore_t*
mutex_create(void)
{
        return semaphore_create(1);
}

void
semaphore_acquire(semaphore_t *semaphore)
{
        task_lock();
        if (semaphore->current_count < semaphore->max_count) {
                ++semaphore->current_count;
                task_unlock();
                return;
        }

        current_task->next = NULL;
        if (!semaphore->waiting_tasks_start)
                semaphore->waiting_tasks_start = current_task;
        else
                semaphore->waiting_tasks_end->next = current_task;
        semaphore->waiting_tasks_end = current_task;

        task_block(WAITING_FOR_LOCK);
        task_unlock();
}

void
mutex_acquire(semaphore_t *mutex)
{
        semaphore_acquire(mutex);
}

void
semaphore_release(semaphore_t *semaphore)
{
        task_lock();
        if (!semaphore->waiting_tasks_start) {
                --semaphore->current_count;
                task_unlock();
                return;
        }

        task_info_t *task = semaphore->waiting_tasks_start;
        semaphore->waiting_tasks_start = task->next;
        task_unblock(task);
        task_unlock();
}

void
mutex_release(semaphore_t *mutex)
{
        semaphore_release(mutex);
}

condvar_t*
condvar_alloc(void *condition)
{
        condvar_t *condvar = kmalloc(sizeof(condvar_t));
        condvar->condition = condition;
        condvar->tasks = NULL;
        condvar->next = NULL;

        condvar->next = condvars;
        condvars = condvar;
        
        return condvar;
}

static void
condvar_add_list(condvar_t *condvar)
{
        task_info_t *task = condvar->tasks;
        for (; task->next; task = task->next) {
                if (task == current_task)
                        return;
        }

        if (task != current_task)
                task->next = current_task;
}

void
condvar_wait(void *condition, semaphore_t *mutex)
{
        task_lock();
        condvar_t *condvar = condvars;
        for (; condvar; condvar = condvar->next) {
                if (condvar->condition != condition)
                        continue;
                         
                current_task->next = NULL;
                if (condvar->tasks)
                        condvar_add_list(condvar);
                else
                        condvar->tasks = current_task;

                mutex_release(mutex);

                task_block(CONDVAR);
                
                task_unlock();
                mutex_acquire(mutex);
                return;
        }

        condvar = condvar_alloc(condition);
        current_task->next = NULL;
        condvar->tasks = current_task;
        
        mutex_release(mutex);
        task_block(CONDVAR);
        
        task_unlock();
        mutex_acquire(mutex);
}

void
condvar_signal(void *condition)
{
        task_lock();
        condvar_t *condvar = condvars;
        for (; condvar; condvar = condvar->next) {
                if (condvar->condition != condition)
                        continue;

                task_info_t *task = condvar->tasks, *next;
                for (; task; task = next) {
                        next = task->next;
                        task_unblock(task);
                }
                        
                condvar->tasks = NULL;
                task_unlock();
                return;
        }
        task_unlock();
}

void
condvar_free(void *condition)
{
        condvar_t *condvar = condvars;
        for (; condvar; condvar = condvar->next) {
                if (condvar->condition != condition)
                        continue;

                kfree(condvar);
                return;
        }
        
        printf("(condvar_release) there isn't such condition in condvar\n");
        abort();
}
