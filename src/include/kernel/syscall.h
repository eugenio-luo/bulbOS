#ifndef _KERNEL_SYSCALL_H
#define _KERNEL_SYSCALL_H

#include <kernel/task.h>
#include <kernel/tss.h>

#define WRMSR(ECX, EAX, EDX)               \
        asm volatile ("movl %0, %%ecx\n\t" \
                      "movl %1, %%eax\n\t" \
                      "movl %2, %%edx\n\t" \
                      "wrmsr\n\t" \
                      : \
                      : "abcdsDi"(ECX), "abdsDi"(EAX), "bdsDi"(EDX))

#define SYSENTER_CS_REG    0x174 
#define SYSENTER_ESP_REG   0x175 
#define SYSENTER_EIP_REG   0x176 

#define SYSCALL2(EAX) \
        asm volatile ("" :: "a"(EAX))
#define SYSCALL3(EAX, EBX) \
        asm volatile ("" :: "a"(EAX), "b"(EBX))
#define SYSCALL4(EAX, EBX, ECX) \
        asm volatile ("" :: "a"(EAX), "b"(EBX), "c"(ECX))
#define SYSCALL5(EAX, EBX, ECX, EDX) \
        asm volatile ("" :: "a"(EAX), "b"(EBX), "c"(ECX), "d"(EDX))
#define SYSCALL6(EAX, EBX, ECX, EDX, ESI) \
        asm volatile ("" :: "a"(EAX), "b"(EBX), "c"(ECX), "d"(EDX), "S"(ESI))
#define SYSCALL7(EAX, EBX, ECX, EDX, ESI, EDI) \
        asm volatile ("" :: "a"(EAX), "b"(EBX), "c"(ECX), "d"(EDX), "S"(ESI), "D"(EDI))

#define VA_LENGTH_(_1,_2,_3,_4,_5,_6,N,...) N
#define VA_LENGTH(...) VA_LENGTH_(0,##__VA_ARGS__,6,5,4,3,2,1,0)

#define SYSCALL(RET, ...) \
        SYSCALL_(VA_LENGTH(__VA_ARGS__), __VA_ARGS__);  \
        syscall_entry(); \
        asm volatile ("mov %%eax, %0\n\t" : "=rm"(RET))

#define SYSCALL_(COUNT, ...) SYSCALL__(COUNT, __VA_ARGS__)
#define SYSCALL__(COUNT, ...) SYSCALL ## COUNT(__VA_ARGS__)

enum {
        SYS_SETUP, /* 0 */
        SYS_EXIT, /* 1 */
        SYS_FORK, /* 2 */
        SYS_READ, /* 3 */
        SYS_WRITE, /* 4 */
        SYS_OPEN, /* 5 */
        SYS_CLOSE, /* 6 */
        SYS_WAITPID, /* 7 */
        SYS_CREAT, /* 8 */
        SYS_LINK, /* 9 */
        SYS_UNLINK, /* 10 */
        SYS_EXECVE, /* 11 */
        SYS_CHDIR, /* 12 */
        SYS_TIME, /* 13 */
        SYS_MKNOD, /* 14 */
        SYS_CHMOD, /* 15 */
        SYS_LCHOWN, /* 16 */
        SYS_STAT = 18, /* 18 */
        SYS_LSEEK, /* 19 */
        SYS_GETPID, /* 20 */
        SYS_MOUNT, /* 21 */
        SYS_OLDMOUNT, /* 22 */
        SYS_SETUID, /* 23 */
        SYS_GETUID, /* 24 */
        SYS_STIME, /* 25 */
        SYS_PTRACE, /* 26 */
        SYS_ALARM, /* 27 */
        SYS_FSTAT, /* 28 */
        SYS_PAUSE, /* 29 */
        SYS_UTIME, /* 30 */
        SYS_ACCESS = 33, /* 33 */
        SYS_NICE, /* 34 */
        SYS_SYNC = 36, /* 36 */
        SYS_KILL, /* 37 */
        SYS_RENAME, /* 38 */
        SYS_MKDIR, /* 39 */
        SYS_RMDIR, /* 40 */
        SYS_DUP, /* 41 */
        SYS_PIPE, /* 42 */
        SYS_READDIR = 460,
        SYSCALL_COUNT, /* not real syscall, only for size purpose */
};

enum {
        O_CREAT = (1 << 0),
        O_RDONLY = (1 << 1),
        O_WRONLY = (1 << 2),
        O_RDWR = (1 << 3),
        O_TRUNC = (1 << 4),
};

#define STDIN   0
#define STDOUT  1

void syscall_init(void);
extern void syscall_entry(void);

#endif
