#include <stdio.h>
#include <stddef.h>

#include <kernel/syscall.h>
#include <kernel/file.h>
#include <kernel/task.h>
#include <kernel/inode.h>
#include <kernel/dir.h>
#include <kernel/pipe.h>

extern void syscall_handler(void);
uintptr_t syscall_table[SYSCALL_COUNT];

static int
syscall_read(int fd, void *buffer, size_t size)
{
        return file_read(current_task->open_files[fd], buffer, size);
}

static int
syscall_write(int fd, void *buffer, size_t size)
{
        return file_write(current_task->open_files[fd], buffer, size);
}

static int
syscall_close(int fd)
{
        file_close(current_task->open_files[fd]);
        current_task->open_files[fd] = 0;
        return 1;
}

static int
syscall_fstat(int fd, stat_t *statbuf)
{
        return file_stat(current_task->open_files[fd], statbuf);
}
        
static int
syscall_dup(int old_fd)
{
        return fd_alloc(current_task->open_files[old_fd]);
}

static int
syscall_link(char *oldpath, char *newpath)
{
        return dir_link(oldpath, newpath);
}

static int
syscall_unlink(char *path)
{
        return dir_unlink(path);
}

static int
syscall_open(char *path, int flags)
{
        return file_open(path, flags);
}

static int
syscall_mkdir(char *path, int mode)
{
        return dir_make(path, mode);
}

static int
syscall_chdir(char *path)
{
        return dir_change(path);
}

static int
syscall_pipe(int pipefd[2])
{
        file_t *read, *write;
        if (pipe_alloc(&read, &write, 1024, PIPE_TYPE_UNBUFFERED))
                return -1;

        int rfd = fd_alloc(read);
        if (rfd == -1) {
                file_close(read);
                file_close(write);
                return -1;
        }

        int wfd = fd_alloc(write);
        if (wfd == -1) {
                current_task->open_files[rfd] = 0;
                file_close(read);
                file_close(write);
                return -1;
        }

        pipefd[0] = rfd;
        pipefd[1] = wfd;
        return 0;
}

int
syscall_not_impl(void)
{
        printf("SYSCALL NOT IMPLEMENTED\n");
        return -1;
}

void
syscall_init(void)
{
        WRMSR(SYSENTER_CS_REG, 0x8, 0);
        WRMSR(SYSENTER_ESP_REG, tss->esp0, 0);
        WRMSR(SYSENTER_EIP_REG, syscall_handler, 0);

        /* tmp solution for not implemented syscalls */
        for (int i = 0; i < SYSCALL_COUNT; ++i)
                syscall_table[i] = (uintptr_t) syscall_not_impl;
        
        syscall_table[SYS_READ] = (uintptr_t) syscall_read; 
        syscall_table[SYS_WRITE] = (uintptr_t) syscall_write; 
        syscall_table[SYS_OPEN] = (uintptr_t) syscall_open; 
        syscall_table[SYS_CLOSE] = (uintptr_t) syscall_close; 
        syscall_table[SYS_LINK] = (uintptr_t) syscall_link; 
        syscall_table[SYS_UNLINK] = (uintptr_t) syscall_unlink; 
        syscall_table[SYS_CHDIR] = (uintptr_t) syscall_chdir; 
        syscall_table[SYS_FSTAT] = (uintptr_t) syscall_fstat; 
        syscall_table[SYS_MKDIR] = (uintptr_t) syscall_mkdir; 
        syscall_table[SYS_DUP] = (uintptr_t) syscall_dup; 
        syscall_table[SYS_PIPE] = (uintptr_t) syscall_pipe; 
}
