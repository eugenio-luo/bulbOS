#include <stdio.h>
#include <stdlib.h>

#include <utils/hashtable.h>

#include <kernel/bio.h>
#include <kernel/mutex.h>
#include <kernel/ide.h>
#include <kernel/vmm.h>

struct {
        hash_table_t *table;
        semaphore_t *mutex;
        bio_buf_t *lhead;
        bio_buf_t *ltail;
} bio_head;

static bio_buf_t*
bio_alloc(int device, uint32_t block, uint32_t size)
{
        bio_buf_t *buf = kmalloc(sizeof(bio_buf_t));
        buf->device = device;
        buf->block = block;
        buf->ref_count = 1;
        buf->valid = 0;
        buf->mutex = mutex_create();
        buf->size = size;
        buf->buffer = kmalloc(size);
        buf->next = NULL;
        buf->prev = NULL;

        return buf;
}

static void
bio_remove(bio_buf_t *buf)
{
        if (!buf->next && !buf->prev) {
                printf("[BIO] buffer not part of list\n");
                abort();
        }
        
        if (!bio_head.lhead && !bio_head.ltail) {
                printf("[BIO] removing from empty list\n");
                abort();
        }
        
        if (buf == bio_head.lhead) {

                bio_head.lhead = buf->next;
                buf->next->prev = NULL;
                
        }

        if (buf == bio_head.ltail) {

                bio_head.ltail = buf->prev;
                buf->prev->next = NULL;
                
        }

        if (buf != bio_head.ltail && buf != bio_head.lhead) {

                buf->prev->next = buf->next;
                buf->next->prev = buf->prev;
                
        }
}

static void
bio_evict(void)
{
        if (!bio_head.ltail) {
                bio_debug_print();
                printf("[BIO] there aren't any free bufs\n");
                abort();
        }
        
        if (bio_head.ltail->ref_count) {
                printf("[BIO] buf shouldn't be already free\n");
                abort();
        }

        bio_buf_t *old = bio_head.ltail;

        hash_key_t key = {((uint64_t)old->device << 32) | old->block};
        if (ht_remove(bio_head.table, key)) {
                printf("[BIO] buf can't free'd in hash table\n");
                abort();
        }
        
        bio_remove(old);
        kfree(old->buffer);
        kfree(old->mutex);
        kfree(old);
}

bio_buf_t*
bio_get(int device, uint32_t block, uint32_t size)
{
        mutex_acquire(bio_head.mutex);
        
        hash_key_t key = {((uint64_t)device << 32) | block}; 
        bio_buf_t *buf = ht_get(bio_head.table, key);

        if (buf) {
                /* TODO: TEMPORARY SOLUTION */
                if (size != buf->size) {
                        printf("[BIO] buf size and required size differ\n");
                        abort();
                }
                
                if (!buf->ref_count++)
                        bio_remove(buf);
               
                mutex_release(bio_head.mutex);
                mutex_acquire(buf->mutex);
                return buf;
        }

        buf = bio_alloc(device, block, size);
        
        if (!ht_set(bio_head.table, key, buf)) {
                mutex_release(bio_head.mutex);
                mutex_acquire(buf->mutex);
                return buf;
        }

        int ret;
        do {
                bio_evict();
                ret = ht_set(bio_head.table, key, buf);
        } while (ret && bio_head.ltail);
        
        if (!ret) {
                mutex_release(bio_head.mutex);
                mutex_acquire(buf->mutex);
                return buf;
        }

        printf("[BIO] can't get any free buffer\n");
        abort();
}

void
bio_release(bio_buf_t *buf)
{
        mutex_acquire(bio_head.mutex);

        if (!--buf->ref_count) {
                if (!bio_head.lhead)
                        bio_head.ltail = buf;
                else
                        bio_head.lhead->prev = buf;

                buf->next = bio_head.lhead;
                bio_head.lhead = buf;
        }
        
        mutex_release(bio_head.mutex);
        mutex_release(buf->mutex);
}

bio_buf_t*
bio_read(int device, uint32_t block, uint32_t size)
{
        bio_buf_t *buf = bio_get(device, block, size);
        if (!buf->valid) {
                ide_read_disk(device, block * size, size);
                ide_read_disk_buffer(buf->buffer, 0, size);
                buf->valid = 1;
        }

        return buf;
}

void
bio_write(bio_buf_t *buf)
{
        ide_write_disk(buf->device, buf->buffer, buf->block * buf->size, buf->size);
        buf->valid = 0;
}

void
bio_debug_print(void)
{
        kprintf("[BIO] hash table %x:\n", bio_head.table->capacity);
        hash_entry_t *entry = bio_head.table->entries;
        kprintf("index | key | value\n");
        for (size_t i = 0; i < bio_head.table->capacity; ++i) {
                if (entry[i].state == HT_VALID)
                        kprintf("%d: x\n", i, entry[i].value);
                else if (entry[i].state == HT_TOMBSTONE)
                        kprintf("%d: tombstone\n", i);
        }
        kprintf("\n");
        
        kprintf("[BIO] next linked list:\n");
        bio_buf_t *buf = bio_head.lhead;
        kprintf("addr | next | prev\n");
        for (; buf; buf = buf->next)
                kprintf("%x, %x, %x\n", buf, buf->next, buf->prev);
        kprintf("\n");
        
        kprintf("[BIO] prev linked list:\n");
        buf = bio_head.ltail;
        kprintf("addr | next | prev\n");
        for (; buf; buf = buf->prev)
                kprintf("%x, %x, %x\n", buf, buf->next, buf->prev);
        kprintf("\n\n");
}

void
bio_init(void)
{
        bio_head.mutex = mutex_create();
        bio_head.lhead = NULL;
        bio_head.ltail = NULL;
        bio_head.table = ht_create(BIO_TABLE_SIZE, BIO_LOAD_FACTOR, 0); 
}
