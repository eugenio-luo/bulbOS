#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <utils/hashtable.h>
#include <kernel/inode.h>
#include <kernel/mutex.h>
#include <kernel/vmm.h>
#include <kernel/dir.h>

struct fs_itf {
        void (*inode_load)(inode_t *);
        void (*inode_update)(inode_t *);
        void (*inode_trunc)(inode_t *);
        uint32_t (*dinode_alloc)(int);
};

struct {
        struct fs_itf fs_itfs[4];
        hash_table_t *table;
        semaphore_t *mutex;
        inode_t *lhead;
        inode_t *ltail;
} ihead;

static inode_t*
inode_alloc(int device, uint32_t inode_n)
{
        inode_t *inode = kmalloc(sizeof(inode_t));
        inode->ref_count = 1;
        inode->valid = 0;
        inode->device = device;
        inode->n = inode_n;
        inode->mutex = mutex_create();

        /* the rest should be allocated by inode_lock */
        return inode;
}

static void
inode_remove(inode_t *inode)
{
        if (!inode->next && !inode->prev) {
                printf("[INODE] inode not part of list\n");
                abort();
        }
        
        if (!ihead.lhead && !ihead.ltail) {
                printf("[INODE] removing from empty list\n");
                abort();
        }
        
        if (inode == ihead.lhead) {
                ihead.lhead = inode->next;
                inode->next->prev = NULL;
        }

        if (inode == ihead.ltail) {
                ihead.ltail = inode->prev;
                inode->prev->next = NULL;
        }

        if (inode != ihead.ltail && inode != ihead.lhead) {
                inode->prev->next = inode->next;
                inode->next->prev = inode->prev;
        }
}

static void
inode_evict(void)
{
        if (!ihead.ltail) {
                printf("[INODE] there aren't any free inodes\n");
                abort();
        }

        if (ihead.ltail->ref_count) {
                printf("[INODE] inode shouldn't be already free\n");
                abort();
        }

        inode_t *old = ihead.ltail;
        hash_key_t key = {((uint64_t)old->device << 32) | old->n};
        
        if (ht_remove(ihead.table, key)) {
                printf("[INODE] inode can't free'd in hash table\n");
                abort();
        }

        inode_remove(old);
        kfree(old->mutex);
        kfree(old);
}

inode_t*
inode_get(int device, uint32_t inode_n)
{
        mutex_acquire(ihead.mutex);

        hash_key_t key = {((uint64_t)device << 32) | inode_n};
        inode_t *inode = ht_get(ihead.table, key);

        if (inode) {
                if (!inode->ref_count++)
                        inode_remove(inode);

                mutex_release(ihead.mutex);
                return inode;
        }

        inode = inode_alloc(device, inode_n);
        
        if (!ht_set(ihead.table, key, inode)) {
                mutex_release(ihead.mutex);
                return inode;
        }

        int ret;
        do {
                inode_evict();
                ret = ht_set(ihead.table, key, inode);
        } while (ret && ihead.ltail);

        if (!ret) {
                mutex_release(ihead.mutex);
                return inode;
        }

        printf("[INODE] can't get any free inode\n");
        abort();
}

inode_t*
dinode_alloc(int device, uint16_t mode)
{
        uint32_t (*alloc)(int);
        alloc = ihead.fs_itfs[device].dinode_alloc;
        uint32_t inode_n = alloc(device);

        inode_t *inode = inode_get(device, inode_n);

        inode_lock(inode);
        
        inode->mode = mode;
        inode->hard_links_count = 1;
        inode->size = 0;
        inode->sectors = 0;
        inode_update(inode);

        return inode;
}

void
inode_release(inode_t *inode)
{
        mutex_acquire(ihead.mutex);

        if (!--inode->ref_count) {
                if (!ihead.lhead)
                        ihead.ltail = inode;
                else
                        ihead.lhead->prev = inode;

                inode->next = ihead.lhead;
                ihead.lhead = inode;

                if (inode->valid && !inode->hard_links_count) {
                        void (*inode_trunc)(inode_t *);
                        inode_trunc = ihead.fs_itfs[inode->device].inode_trunc;
                        inode_trunc(inode);
                        inode->valid = 0;
                }
        }

        mutex_release(ihead.mutex);
}

inode_t*
inode_dup(inode_t *inode)
{
        mutex_acquire(ihead.mutex);
        ++inode->ref_count;
        mutex_release(ihead.mutex);
        return inode;
}

void
inode_lock(inode_t *inode)
{
        if (!inode || !inode->ref_count) {
                printf("%d\n", inode->ref_count);
                printf("[INODE][inode_lock] inode in wrong state\n");
                abort();
        }
        
        mutex_acquire(inode->mutex);

        if (!inode->valid) {
                void (*load)(inode_t *) = ihead.fs_itfs[inode->device].inode_load;
                load(inode);
        }
}

void
inode_unlock(inode_t *inode)
{
        if (!inode || !inode->ref_count) {
                printf("%d\n", inode->ref_count);
                printf("[INODE][inode_unlock] inode in wrong state\n");
                abort();
        }
        
        mutex_release(inode->mutex);
}    

void
inode_stat(file_t *file, stat_t *stat)
{
        stat->device = file->inode->device;
        stat->inode_n = file->inode->n;
        stat->hlinks = file->inode->hard_links_count;
        stat->mode = file->inode->mode;
        stat->size = file->inode->size;
        stat->last_access_time = file->inode->last_access_time;
        stat->creation_time = file->inode->creation_time;
        stat->last_modify_time = file->inode->last_modify_time;
        stat->deletion_time = file->inode->deletion_time;
}

void
inode_close(file_t *file)
{
        inode_release(file->inode);
}

int
inode_read(file_t *file, void *addr, size_t size)
{
        int (*read)(inode_t *, void *, size_t, size_t);
        read = file->inode->read;
        
        int bread = read(file->inode, addr, file->offset, size);
        if (bread)
                file->offset += bread;

        return bread;
}

int
inode_write(file_t *file, void *addr, size_t size)
{
        int (*write)(inode_t *, void *, size_t, size_t);
        write = file->inode->write;

        int bwrite = write(file->inode, addr, file->offset, size);
        if (bwrite)
                file->offset += bwrite;

        return bwrite;
}

void
inode_update(inode_t *inode)
{
        void (*update)(inode_t *);
        update = ihead.fs_itfs[inode->device].inode_update;
        update(inode);
}

void
inode_trunc(inode_t *inode)
{
        void (*trunc)(inode_t *);
        trunc = ihead.fs_itfs[inode->device].inode_trunc;
        trunc(inode);
}

void
inode_add_itf(int device, 
              void (*inode_load)(inode_t *),
              void (*inode_update)(inode_t *),
              void (*inode_trunc)(inode_t *),
              uint32_t (*dinode_alloc)(int))
{
        ihead.fs_itfs[device].inode_load = inode_load;
        ihead.fs_itfs[device].inode_update = inode_update;
        ihead.fs_itfs[device].inode_trunc = inode_trunc;
        ihead.fs_itfs[device].dinode_alloc = dinode_alloc;
}

void
inode_init(void)
{
        ihead.mutex = mutex_create();
        ihead.lhead = NULL;
        ihead.ltail = NULL;
        ihead.table = ht_create(INODE_TABLE_SIZE, INODE_LOAD_FACTOR, 0); 
}
