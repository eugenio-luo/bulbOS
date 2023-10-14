#ifndef _KERNEL_FILE_H
#define _KERNEL_FILE_H

#include <kernel/inode.h>
#include <kernel/filesystem.h>
#include <kernel/mutex.h>
#include <kernel/pipe.h>

typedef struct stat {
        int device;
        uint32_t inode_n;
        uint32_t hlinks;
        uint32_t mode;
        uint32_t size;
        uint32_t last_access_time;
        uint32_t creation_time;
        uint32_t last_modify_time;
        uint32_t deletion_time;
} stat_t;

typedef struct file {
        uint32_t ref_count;
        uint16_t readp;
        uint16_t writep;
        union {
                struct pipe *pipe;
                struct inode *inode;
        };
        union {
                uint32_t offset;
                uint32_t major;
        };

        void (*close)(struct file *);
        int  (*read) (struct file *, void *, size_t);
        int  (*write)(struct file *, void *, size_t);
        void (*stat) (struct file *, stat_t *);
        
        struct file *next;
} file_t;

int inode_write(file_t *file, void *addr, size_t size);
int inode_read(file_t *file, void *addr, size_t size);
void inode_stat(file_t *file, stat_t *stat);

void file_init(void);
int file_open(char *path, int flags);
int file_write(file_t *file, void *addr, size_t size);
int file_read(file_t *file, void *addr, size_t size);
file_t* file_alloc(void);
void file_close(file_t *file);
int file_stat(file_t *file, stat_t *statbuf);

int fd_alloc(file_t *file);

#endif
