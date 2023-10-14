#ifndef _KERNEL_STDIO_HANDLER_H
#define _KERNEL_STDIO_HANDLER_H

#include <kernel/file.h>
#include <kernel/keyboard.h>

extern file_t *stdin_read;
extern file_t *stdout_write;

#define SCREEN_SIZE    (80 * 25)

void stdio_init(void);
void stdin_send_code(kb_keysym_t keycode);

#endif
