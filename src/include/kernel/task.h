#ifndef _KERNEL_TASK_H
#define _KERNEL_TASK_H

#include <stdint.h>

#include <kernel/file.h>
#include <kernel/dir.h>

#define OPEN_FILES_COUNT   16

typedef struct task_info {
        uint32_t pid;
        uint32_t esp;
        uint32_t esp0;
        uint32_t *ebp;
        uint32_t page_dir;
        uint32_t state;
        struct task_info *next;
        struct dentry *current_dir;
        struct file *open_files[OPEN_FILES_COUNT];
        uint64_t time_used;
        uint64_t wake_up_time;
        char     name[8];
} __attribute__((packed)) task_info_t;

typedef enum {
        AVAILABLE = 0,
        RUNNING, 
        BLOCKED,
        SLEEPING,
        TERMINATED,
        WAITING_FOR_LOCK,
        IO_REQUEST,
        CONDVAR,
} task_state_t;

extern task_info_t *current_task;

void multitask_init(void);

void task_add_node(task_info_t *task);
task_info_t *task_kernel_create_new(void (*func)(), char *name);
task_info_t *task_user_create_new(void (*func)(), char *name);
void task_force_switch(task_info_t *task);

void task_lock(void);
void task_unlock(void);
void task_block(task_state_t reason);
void task_unblock(task_info_t *task);
void nano_sleep_until(uint64_t ns);

void task_print_info(void);
void task_print_list(task_info_t *start);

#endif
