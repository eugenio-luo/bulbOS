#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <kernel/task.h>
#include <kernel/vmm.h>
#include <kernel/page.h>
#include <kernel/hpet.h>
#include <kernel/isrs.h>
#include <kernel/idt.h>
#include <kernel/apic.h>
#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/stdio_handler.h>
#include <kernel/loader.h>

/* time slice for now is a random number */
#define TIME_SLICE              10
#define INIT_FUNC_PTR(task)     (task->ebp - 2)

static void task_terminate(void);
/* kernel/arch/i386/task_switch.S */
extern void task_switch(task_info_t *new);
/* kernel/arch/i386/ring3.S */
extern void enter_ring3(void);

task_info_t *current_task = NULL;
static task_info_t *schedule_tasks = NULL;
static task_info_t *sleep_tasks = NULL;
static task_info_t *terminated_tasks = NULL;

static task_info_t *task_cleaner = NULL;
static task_info_t *idle_task = NULL;

static uint32_t pid_count = 1;

/* used for time keeping */
static uint64_t last_count = 0;

static uint32_t schedule_lock_counter = 0;
static uint32_t postpone_switch_counter = 0;
static uint32_t postpone_switch_flag = 0;

static uint64_t time_slice_remaining = TIME_SLICE;

static task_info_t*
task_create_new(void (*func)(), char *name)
{
        task_info_t *new_task = (task_info_t*) kmalloc(sizeof(task_info_t));

        new_task->pid = pid_count++;
        new_task->ebp = kmalloc(PAGE_FRAME_SIZE);
        /* the stack starts from higher address */
        new_task->ebp += PAGE_LAST_DWORD;
        uint32_t *stack = new_task->ebp;

        /* the stack grows toward lower address */
        *(stack) = (uintptr_t) task_terminate;
        *(--stack) = (uintptr_t) func; /* return pointer */

        /* a user task and kernel task has different init function, 
           so temporary the init function is NULL */
        *(--stack) = (uintptr_t) NULL;

        *(--stack) = (uintptr_t) new_task->ebp; /* ebp */
        *(--stack) = 0;     /* ebx */
        *(--stack) = 0;     /* esi */
        *(--stack) = 0;     /* edi */

        new_task->esp = (uintptr_t) stack; 

        new_task->esp0 = (uintptr_t) kmalloc(PAGE_FRAME_SIZE);
        new_task->esp0 += PAGE_LAST_DWORD;

        /* TODO: create copy of page_dir */
        new_task->page_dir = (uintptr_t) NULL;
        new_task->state = AVAILABLE;
        new_task->time_used = 0;
        new_task->wake_up_time = 0;
        new_task->current_dir = NULL;
        new_task->next = NULL;
        memcpy(&new_task->name, name, 8);
        
        return new_task;
}

void
task_lock_scheduler(void)
{
        ++schedule_lock_counter;
        
        DPRINTF("[TASK] schedule locked: %x\n", schedule_lock_counter);
}

void
task_unlock_scheduler(void)
{
        if (!schedule_lock_counter--) {
                printf("[TASK] unlocking nothing\n");
                abort();
        }
        
        DPRINTF("[TASK] schedule unlocked: %x\n", schedule_lock_counter);
}

static void
task_user_init(void)
{
        task_unlock_scheduler();
        enter_ring3();

        /* task then will enter into the main function */
}

/* TODO: executable loader */
task_info_t*
task_user_create_new(void (*func)(), char *name)
{
        task_info_t *task = task_create_new(func, name);

        task->open_files[0] = stdin_read;
        task->open_files[1] = stdout_write;
        
        uint32_t *init_function = INIT_FUNC_PTR(task);
        *init_function = (uintptr_t) task_user_init;

        task->page_dir = page_get_phys_addr(page_directory, (uintptr_t)page_directory);
        /* havent create it yet */
        // task->page_dir = load_new_page_dir();
        DPRINTF("user task %d created\n", task->pid);
        return task;
}

static void
task_kernel_init(void)
{
        task_unlock_scheduler();
}    

task_info_t*
task_kernel_create_new(void (*func)(), char *name)
{
        task_info_t *task = task_create_new(func, name);

        uint32_t *init_function = INIT_FUNC_PTR(task);
        *init_function = (uintptr_t) task_kernel_init;
        
        task->page_dir = page_get_phys_addr(page_directory, (uintptr_t)page_directory);
        DPRINTF("kernel task %d created\n", task->pid);
        return task;
}

void
task_add_node(task_info_t *task)
{
        task->state = AVAILABLE;

        /* adds quickly but the first task in list might never get time */
        task->next = schedule_tasks;
        schedule_tasks = task;
      
        DPRINTF("[TASK] task %d added to schedule list\n", task->pid);
        //task_print_list(schedule_tasks);
}

static void
task_switch_wrapper(task_info_t *task)
{
        if (postpone_switch_counter) {
                DPRINTF("[TASK] switch not happening to task %d\n", task->pid);
                DPRINTF("[TASK] postpone flag UP\n");
                postpone_switch_flag = 1;

                /* add task to list so the postponed switch is possible */
                task_add_node(task);
                return;
        }

        if (current_task == idle_task)
                task_add_node(current_task);
                       
        /* there isn't any need to schedule if the only task is the idle task */
        time_slice_remaining = (task == idle_task) ? 0 : TIME_SLICE;

        DPRINTF("new time slice: %x\n", time_slice_remaining);
        DPRINTF("[TASK] switching to task %d\n", task->pid);

        task_switch(task);
}

void
task_force_switch(task_info_t *task)
{
        task_lock_scheduler();
        current_task->next = schedule_tasks;
        schedule_tasks = current_task;

        task_switch_wrapper(task);
        task_unlock_scheduler();
}

static void
task_update_counter(void)
{
        uint64_t current_count = hpet_read_counter();
        uint64_t elapsed_time = (current_count - last_count) * 10;
        last_count = current_count;
        
        current_task->time_used += elapsed_time;
}

// TODO: introduce UNIX scheduler instead of awful round robin
static void
task_schedule(void)
{
        task_update_counter();

        if (postpone_switch_counter) {
                postpone_switch_flag = 1;
                return;
        }

        if (!schedule_tasks) {
                DPRINTF("[TASK] no task ready to switch to\n");
                return;
        }

        task_print_list(schedule_tasks);
        
        task_info_t *task = schedule_tasks;
        schedule_tasks = task->next;
        DPRINTF("[TASK] ready to switch to task %d\n", task->pid);

        /* it doesn't want to immediately switch to idle task, first check if
           there are other possibilities */
        if (task == idle_task) {
                /* 1. check if there are other task on the scheduled list */
                if (schedule_tasks) {
                        task = schedule_tasks;
                        idle_task->next = task->next;
                        schedule_tasks = idle_task;
                        DPRINTF("[TASK] instead of idle, there is task %d\n", task->pid);

                /* 2. check if current task can still run and it wasn't blocked */
                } else if (current_task->state == RUNNING) {
                        DPRINTF("[TASK] instead of idle, continuing running "
                                "current task %d\n", current_task->pid);
                        return;

                }
        }

        DPRINTF("[TASK] current task state: %x\n", current_task->state);
        DPRINTF("[TASK] switch called by scheduling\n");
        task_switch_wrapper(task);
}

void
task_lock(void)
{
        task_lock_scheduler();
        ++postpone_switch_counter;
}

void
task_unlock(void)
{
        /* if postpone flag isn't set, it means there isn't any task
           switch that was postponed, so it simply decrease postpone counter */
        if (!--postpone_switch_counter && postpone_switch_flag) {
                postpone_switch_flag = 0;
                task_schedule();
        }

        task_unlock_scheduler();
}

void
task_block(task_state_t reason)
{
        task_lock_scheduler();
        current_task->state = reason;
        task_schedule();
        task_unlock_scheduler();
}

void
task_unblock(task_info_t *task)
{
        task_lock_scheduler();
        
        /* there is immediate switch if there is only one task
           or the current task is the idle task */
        if (!schedule_tasks || current_task == idle_task) {
                DPRINTF("[TASK] switch called by unblock\n");
                task_switch_wrapper(task);
        } else
                task_add_node(task);
                
        task_unlock_scheduler();
}

static void
task_idle(void)
{
        for (;;) 
                asm volatile ("hlt");
}

static void
task_cleanup(task_info_t *task)
{
        DPRINTF("[TASK] cleaning up task %d\n", task->pid);
        kfree(task->ebp - PAGE_LAST_DWORD);
        kfree(task);
}

static void
task_clean(void)
{
        /* if there wasn't a infinite loop, the task would terminated
           and try to clean itself up, crashing the OS probably */
        for (;;) {
                task_lock();

                while (terminated_tasks) {
                        task_info_t *task = terminated_tasks;
                        terminated_tasks = task->next;
                        task_cleanup(task);
                }
                
                task_unlock();

                /* finished cleaning, it get blocked immediately and switch
                   to other tasks */
                task_block(BLOCKED);
        }
}

static void
task_terminate(void)
{
        task_lock();
        task_lock_scheduler();

        /* when a task get terminated it is simply blocked with TERMINATED
           state, then put on a list */
        current_task->next = terminated_tasks;
        terminated_tasks = current_task;
        task_unlock_scheduler();

        DPRINTF("[TASK] task %d has been terminated\n", current_task->pid);
        task_block(TERMINATED);

        /* when possible it will be cleaned up */
        task_unblock(task_cleaner);

        task_unlock();
}

void
nano_sleep_until(uint64_t wake_up_time_ns)
{
        task_lock();

        /* if sleep is so small that wake up time already passed, return
           instead of wasting time */
        if (wake_up_time_ns < hpet_get_ns()) {
                task_unlock();
                return;
        }

        current_task->wake_up_time = wake_up_time_ns;

        DPRINTF("[TASK] task %d has been put to sleep\n", current_task->pid);

        current_task->next = sleep_tasks;
        sleep_tasks = current_task;

        task_unlock();
        task_block(SLEEPING);       
}


__attribute__ ((interrupt))
void
task_time_handler(interrupt_frame_t *frame)
{
        (void) frame; /* suppress compiler warning */
        
        /* if locked, send EOI and return */
        if (schedule_lock_counter) {
                DPRINTF("[TASK] schedule blocked: %d\n", schedule_lock_counter);
                lapic_sendEOI();
                return;
        }
        
        task_lock();

        task_info_t *next_task = sleep_tasks;
        sleep_tasks = NULL;
        uint64_t actual_time = hpet_get_ns();

        /* for debug, it doesn't have any function */
        if (next_task) {
                DPRINTF("[TASK] sleep list: ");
                task_print_list(sleep_tasks);
        }

        /* checking if any task can be woken up */
        while (next_task) {
                task_info_t *task = next_task;
                next_task = task->next;

                if (task->wake_up_time <= actual_time) {
                        DPRINTF("[TASK] task %d has been woken up\n", task->pid);
                        task_unblock(task);
                } else {
                        DPRINTF("[TASK] task %d can't be woken up yet\n", task->pid);
                        task->next = sleep_tasks;
                        sleep_tasks = task;
                }
        }
        
        /* if time slice remaining = 0 then it means there's no need to 
           do any scheduling because there's only one task (current one or idle) */
        if (time_slice_remaining && !--time_slice_remaining) {
                lapic_sendEOI();
                task_schedule();
        }

        lapic_sendEOI();
        task_unlock();
}
       
void
multitask_init(void)
{
        kprintf("[TASK] setup STARTING\n");

        current_task = kmalloc(sizeof(task_info_t)); 
        current_task->pid = pid_count++;

        current_task->esp = 0;
        current_task->esp0 = 0;
       
        current_task->page_dir = page_get_phys_addr(page_directory, (uintptr_t)page_directory);
        current_task->state = RUNNING;
        current_task->time_used = 0;
        current_task->next = NULL;
        memcpy(&current_task->name, "kernel", 7);
        
        /* task cleaner cleans up terminated tasks and free its memory when possible */
        task_cleaner = task_kernel_create_new(task_clean, "clean");
        idle_task = task_kernel_create_new(task_idle, "idle");
        task_add_node(idle_task);

        kprintf("[TASK] kernel main task PID: %x\n", current_task->pid);
        kprintf("[TASK] task cleaner PID: %x\n", task_cleaner->pid);
        kprintf("[TASK] idle task PID: %x\n", idle_task->pid);
        
        kprintf("[TASK] setup COMPLETE\n");
}

/* functions used for debug */
void
task_print_info(void)
{
#ifdef DEBUG
        kprintf("[TASK][INFO] irq_disable_counter: %x, ", schedule_lock_counter);
        kprintf("postpone_switch_counter: %x, ", postpone_switch_counter);
        kprintf("postpone_switch_flag: %x, ", postpone_switch_flag);
        kprintf("idle time: %x%x, ", idle_task->time_used);
        kprintf("time slice remaining: %d\n", time_slice_remaining);
#endif
}

void
task_print_list(task_info_t *start)
{
#ifdef DEBUG
        task_info_t *task = start;

        kprintf("[TASK][INFO] list: ");
        for (; task; task = task->next)
                kprintf("%d ", task->pid);

        kprintf("\n");
#else
        (void) start;
#endif
}

