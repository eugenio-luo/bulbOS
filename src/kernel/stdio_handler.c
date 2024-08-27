#include <stdio.h>
#include <stddef.h>

#include <kernel/vmm.h>
#include <kernel/stdio_handler.h>
#include <kernel/pipe.h>
#include <kernel/file.h>
#include <kernel/mutex.h>
#include <kernel/task.h>
#include <kernel/keyboard.h>

file_t *stdin_read = NULL;
file_t *stdout_write = NULL;

static file_t *stdin_write;
static file_t *stdout_read;

task_info_t *stdout_task = NULL;
task_info_t *stdin_task = NULL;

static uint8_t *stdout_buffer;
static uint8_t *stdin_buffer;

static kb_keysym_t last_code;

static void
stdout_handler(void)
{
        for (;;) {
                int b_read = file_read(stdout_read, stdout_buffer, '\0');
                for (int i = 0; i < b_read; ++i)
                        putchar(stdout_buffer[i]);
                
                mutex_acquire(stdout_read->pipe->mutex);
                condvar_wait(&stdout_read->pipe->b_read, stdout_read->pipe->mutex);
                mutex_release(stdout_read->pipe->mutex);
        }
}

static void
stdin_keysym_to_char(void)
{
        int code_int = (last_code.flags << 16) | last_code.keysym;
        
        if (code_int >= SYM_a && code_int <= SYM_z) {
                last_code.ascii = 'a' + code_int - SYM_a;
                return;
        }

        if (code_int >= SYM_A && code_int <= SYM_Z) {
                last_code.ascii = 'A' + code_int - SYM_A;
                return;
        }
        
        switch (code_int) {
        case SYM_SLASH:
                last_code.ascii = '/';
                break;

        case SYM_PERIOD:
                last_code.ascii = '.';
                break;

        case SYM_SPACE:
                last_code.ascii = ' ';
                break;
                
        case SYM_DASH:
                last_code.ascii = '-';
                break;
                
        case SYM_ENTER:
                last_code.ascii = '\n';
                break;

        case SYM_BACKSPACE:
                last_code.ascii = '\b';
                
        default:
                break;
        }
}

static void
stdin_handler(void)
{
        for (;;) {
                stdin_keysym_to_char();
                file_write(stdin_write, &last_code, 4);
                task_block(BLOCKED);
        }
}

void
stdin_send_code(kb_keysym_t keycode)
{
        last_code = keycode;
        task_force_switch(stdin_task);
}

static void
stdin_init(void)
{
        pipe_alloc(&stdin_read, &stdin_write, PIPE_SIZE, PIPE_TYPE_UNBUFFERED);
        stdin_buffer = kmalloc(10);
        stdin_task = task_kernel_create_new(stdin_handler, "stdin");
}

static void
stdout_init(void)
{
        pipe_alloc(&stdout_read, &stdout_write, PIPE_SIZE, PIPE_TYPE_BUF_CHAR);
        stdout_buffer = kmalloc(PIPE_SIZE);
        stdout_task = task_kernel_create_new(stdout_handler, "stdout");
        task_add_node(stdout_task);
}

void
stdio_init(void)
{
        stdin_init();
        stdout_init();
}
