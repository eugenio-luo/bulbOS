#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kernel/shell.h>
#include <kernel/syscall.h>
#include <kernel/vmm.h>
#include <kernel/keyboard.h>
#include <kernel/dir.h>
#include <kernel/task.h>

static void
shell_input(char *buffer, size_t size)
{
        kb_keysym_t keysym = {0};
        int ret = 0;
        
        for (size_t i = 0; i < size; ) {
                SYSCALL(ret, SYS_READ, 0, &keysym, sizeof(kb_keysym_t));

                switch (keysym.ascii) {
                case 0:
                        continue;
                        
                case '\n':
                        buffer[i] = 0;
                        SYSCALL(ret, SYS_WRITE, 1, &keysym.ascii, 1);
                        return;
                        
                case '\b':
                        if (!i)
                                continue;
                        --i;
                        break;

                default:
                        buffer[i++] = keysym.ascii;
                }

                SYSCALL(ret, SYS_WRITE, 1, &keysym.ascii, 1);
        }

}

static void
shell_ls(void)
{
        int ret = 0;
        const char *str = "list files\n";
        SYSCALL(ret, SYS_WRITE, 1, str, strlen(str));
}

static void
shell_parse(char *buffer)
{
        int ret = 0;
        
        switch (buffer[0]) {
        case 'l':
                if (!memcmp(buffer, "ls", 2))
                        shell_ls();  
                break;
                
        default: {
                for (int i = 0; ; ++i) {
                        if (buffer[i] == 0)
                                break;
                        
                        SYSCALL(ret, SYS_WRITE, 1, &buffer[i], 1);
                }
                SYSCALL(ret, SYS_WRITE, 1, &"\n"[0], 1);
                break;
        }
        }
}

void
shell_main(void)
{
        char *buffer = kmalloc(512);
        int ret;

        current_task->current_dir = dir_get("/");
        dentry_t *cur_dir = current_task->current_dir;
        size_t dir_len = strlen(cur_dir->path);
        
        while (1) {
                SYSCALL(ret, SYS_WRITE, 1, "> ", 2);
                shell_input(buffer, 512);
                shell_parse(buffer);
        }
}
