#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <kernel/vmm.h>
#include <kernel/file.h>
#include <kernel/mutex.h>
#include <kernel/filesystem.h>
#include <kernel/pipe.h>
#include <kernel/syscall.h>

struct {
        semaphore_t *mutex;
        file_t *flist;
        file_t *ulist;
} fhead;

file_t*
file_alloc(void)
{
        mutex_acquire(fhead.mutex);

        file_t *file;
        if (fhead.flist) {
                file = fhead.flist;
                fhead.flist = file->next;
        } else {
                file = kmalloc(sizeof(file_t)); 
        }
        
        file->ref_count = 1;
        file->next = fhead.ulist;
        fhead.ulist = file;

        file->close = NULL;
        file->read = NULL;
        file->write = NULL;
        file->stat = NULL;
        
        mutex_release(fhead.mutex);
        return file;
}

file_t*
file_dup(file_t *file)
{
        mutex_acquire(fhead.mutex);
        ++file->ref_count;
        mutex_release(fhead.mutex);

        return file;
}

void
file_close(file_t *file)
{
        mutex_acquire(fhead.mutex);

        if (--file->ref_count > 0) {
                mutex_release(fhead.mutex);
                return;
        }

        memset(file, 0, sizeof(file_t));
        file->next = fhead.flist;
        fhead.flist = file;
        void (*close)(file_t *) = file->close;
        
        mutex_release(fhead.mutex);
        close(file);
}

int
file_read(file_t *file, void *addr, size_t size)
{
        if (!file->readp)
                return -1;

        if (!file->read)
                return -1;
        
        int (*read)(file_t *, void *, size_t) = file->read;
        int bread = read(file, addr, size);
        return bread;
}

int
file_write(file_t *file, void *addr, size_t size)
{
        if (!file->writep)
                return -1;

        if (!file->write)
                return -1;
        
        int (*write)(file_t *, void *, size_t) = file->write;
        int bwrite = write(file, addr, size);
        return bwrite;
}

int
file_stat(file_t *file, stat_t *statbuf)
{
        if (!file->stat)
                return -1;
        
        void (*stat)(file_t *, stat_t *) = file->stat;
        stat(file, statbuf);
        return 0;
}

int
fd_alloc(file_t *file)
{
        for (int i = 0; i < OPEN_FILES_COUNT; ++i) {
                if (!current_task->open_files[i]) {
                        current_task->open_files[i] = file_dup(file);
                        return i;
                }
        }

        return -1;
}

int
file_open(char *path, int flags)
{
        dentry_t *dentry = dir_get(path); 
        if (!dentry && (flags & O_CREAT))
                dentry = dir_create(path, EXT2_TYPE_FILE, 0);
                
        if (!dentry)
                return -1;

        inode_t *inode = dentry->inode;
        inode_lock(inode);

        if ((inode->mode & EXT2_TYPE_DIR) && flags != O_RDONLY) {
                inode_unlock(inode);
                dir_release(dentry);
                return -1;
        }

        file_t *file = file_alloc();
        int fd = fd_alloc(file);
        if (fd == -1) {
                inode_unlock(inode);
                dir_release(dentry);
                file_close(file);
                return -1;
        }

        file->inode = inode;
        file->offset = 0;
        
        file->close = file_close;
        file->write = inode_write;
        file->read = inode_read;
        file->stat = inode_stat;

        file->readp = !(flags & O_WRONLY);
        file->writep = (flags & O_WRONLY) || (flags & O_RDWR);

        if ((flags & O_TRUNC) && (inode->mode & EXT2_TYPE_FILE))
                inode_trunc(inode);

        inode_unlock(inode);
        return fd;
}

void
file_init(void)
{
        fhead.mutex = mutex_create();
        fhead.flist = NULL;
        fhead.ulist = NULL;
}
