#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kernel/shell.h>
#include <kernel/syscall.h>
#include <kernel/vmm.h>
#include <kernel/keyboard.h>
#include <kernel/dir.h>
#include <kernel/task.h>
#include <kernel/dirent.h>

#define KPRINTF(RET, STRING) SYSCALL(RET, SYS_WRITE, STDOUT, STRING, strlen(STRING))

#define ERR_CMD1 "no known command \""
#define ERR_CMD2 "\"\n"

static void
shell_input(char *buffer, size_t size)
{
        kb_keysym_t keysym = {0};
        int ret = 0;
        
        for (size_t i = 0; i < size; ) {
                SYSCALL(ret, SYS_READ, STDIN, &keysym, sizeof(kb_keysym_t));

                switch (keysym.ascii) {
                case 0:
                        continue;
                        
                case '\n':
                        buffer[i] = 0;
                        SYSCALL(ret, SYS_WRITE, STDOUT, &keysym.ascii, 1);
                        return;
                        
                case '\b':
                        if (!i)
                                continue;
                        --i;
                        break;

                default:
                        buffer[i++] = keysym.ascii;
                }

                SYSCALL(ret, SYS_WRITE, STDOUT, &keysym.ascii, 1);
        }

}

static void
shell_cd(char *buffer)
{
        if (buffer[2] == '\0' || buffer[3] == '\0') return;
        
        int ret = 0;
        SYSCALL(ret, SYS_CHDIR, &buffer[3]); 
}

static void
shell_ls(void)
{
        int ret = 0;
        struct dirent *entries = NULL;

        SYSCALL(ret, SYS_READDIR, &entries, ""); 
        SYSCALL(ret, SYS_WRITE, STDOUT, "\n", 1);
        
        struct dirent *tmp = entries;
        for (struct dirent *tmp = entries; tmp != NULL; tmp = tmp->next) {
            SYSCALL(ret, SYS_WRITE, STDOUT, tmp->name, strlen(tmp->name));
            SYSCALL(ret, SYS_WRITE, STDOUT, "\n", 1);
        }

        SYSCALL(ret, SYS_WRITE, STDOUT, "\n", 1);
        
        for (; entries != NULL; entries = entries->next) {
            kfree(entries);
        }
}

static void
shell_echo(char *buffer)
{
        if (buffer[4] == '\0' || buffer[5] == '\0') return;

        int ret = 0;
        SYSCALL(ret, SYS_WRITE, STDOUT, &buffer[5], strlen(&buffer[5]));
        SYSCALL(ret, SYS_WRITE, STDOUT, &"\n", 1);
}

static void
shell_cat(char *buffer)
{
        if (buffer[3] == '\0' || buffer[4] == '\0') return;
        
        int fd = 0, ret = 0;
        SYSCALL(fd, SYS_OPEN, &buffer[4], O_RDONLY);
        if (fd == -1) {
            KPRINTF(ret, "file not found\n");
        }
        
        stat_t *stat = kmalloc(sizeof(stat_t));
        SYSCALL(ret, SYS_FSTAT, fd, stat);
        
        char *buf = kmalloc(sizeof(stat->size));
        SYSCALL(ret, SYS_READ, fd, buf, stat->size);
        SYSCALL(ret, SYS_WRITE, STDOUT, buf, stat->size);
        
        SYSCALL(ret, SYS_CLOSE, fd);
}

static void
shell_parse(char *buffer)
{
        if (buffer[0] == '\0') return;

        int ret = 0;
        
        switch (buffer[0]) {
        case 'l':
                if (!memcmp(buffer, "ls", 2)) {
                        shell_ls();
                } else {
                        goto shell_input_error;
                }
                break;
                
        case 'e':
                if (!memcmp(buffer, "echo", 4)) {
                        shell_echo(buffer);
                } else {
                        goto shell_input_error;
                }
                break;

        case 'c':
                if (!memcmp(buffer, "cd", 2)) {
                        shell_cd(buffer);
                } else if (!memcmp(buffer, "cat", 3)) {
                        shell_cat(buffer);
                } else {
                        goto shell_input_error;
                }
                break;

        default: {
shell_input_error:
                SYSCALL(ret, SYS_WRITE, STDOUT, ERR_CMD1, strlen(ERR_CMD1));
                SYSCALL(ret, SYS_WRITE, STDOUT, buffer, strlen(buffer));
                SYSCALL(ret, SYS_WRITE, STDOUT, ERR_CMD2, strlen(ERR_CMD2));
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
       
        while (1) {
                dentry_t *cur_dir = current_task->current_dir;
                size_t dir_len = strlen(cur_dir->path);
 
                SYSCALL(ret, SYS_WRITE, STDOUT, cur_dir->path, dir_len);
                SYSCALL(ret, SYS_WRITE, STDOUT, " > ", 3);
                shell_input(buffer, 512);
                shell_parse(buffer);
        }
}
