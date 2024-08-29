#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <utils/hashtable.h>

#include <kernel/dir.h>
#include <kernel/inode.h>
#include <kernel/mutex.h>
#include <kernel/vmm.h>

struct dir_itf {
        int valid;
        int (*parse_dir)(dentry_t *);
        int (*write_dir)(dentry_t *, dentry_t *, char *, int);
        void (*parse_root)(int);
};

static struct {
        struct dir_itf dir_itfs[4];
        hash_table_t *table;
        semaphore_t *mutex;
        dentry_t *lhead;
        dentry_t *ltail;
} dir_head;

static dentry_t*
dentry_alloc(char *path, inode_t *inode, uint32_t offset)
{
        dentry_t *dentry = kmalloc(sizeof(dentry_t));
        dentry->valid = 0;
        dentry->ref_count = 1;
        dentry->offset = offset;
        dentry->inode = inode_dup(inode);
        dentry->mutex = mutex_create();

        dentry->path = path;
        /*
        size_t len = strlen(path);
        dentry->path = kmalloc(len);
        memcpy(dentry->path, path, len);
        */

        dentry->next = NULL;
        dentry->prev = NULL;

        /* these should be allocated by each implementation of parse_dir */
        return dentry;
}

static void
dir_remove(dentry_t *dentry)
{
        if (!dentry->next && !dentry->prev && dir_head.lhead != dentry) {
                printf("dentry->next: %x, dentry->prev: %x\n", dentry->next, dentry->prev);
                printf("dentry->inode->n: %d\n", dentry->inode->n);
                printf("[DENTRY] dentry not part of list\n");
                abort();
        }
        
        if (!dir_head.lhead && !dir_head.ltail) {
                printf("[DENTRY] removing from empty list\n");
                abort();
        }
        
        if (dentry == dir_head.lhead) {
                dir_head.lhead = dentry->next;
                dentry->next->prev = NULL;
        }

        if (dentry == dir_head.ltail) {
                dir_head.ltail = dentry->prev;
                dentry->prev->next = NULL;
        }

        if (dentry != dir_head.ltail && dentry != dir_head.lhead) {
                dentry->prev->next = dentry->next;
                dentry->next->prev = dentry->prev;
        }
}

/* HELPER FUNCTION */
static dentry_t* 
_dir_dup(dentry_t *dentry)
{
        ++dentry->ref_count;
        return dentry;
}

dentry_t*
dir_dup(dentry_t *dentry)
{
        mutex_acquire(dir_head.mutex);
        _dir_dup(dentry);
        mutex_release(dir_head.mutex);
        return dentry;
}

void
dir_add_child(dentry_t *dir, dentry_t *entry)
{
        hash_key_t key = {.key32 = entry->path};
        if (ht_get(dir->table, key)) {
            return;
        }

        ht_set(dir->table, key, entry);

        entry->parent = _dir_dup(dir);

        if (dir->children)
                dir->children->prev_sib = entry;
        
        entry->next_sib = dir->children;
        entry->prev_sib = NULL;
        dir->children = _dir_dup(entry);
}

/* HELPER FUNCTION */
static void
_dir_release(dentry_t *dentry)
{
        if (!--dentry->ref_count) {
                if (!dir_head.lhead)
                        dir_head.ltail = dentry;
                else
                        dir_head.lhead->prev = dentry;

                dentry->next = dir_head.lhead;
                dir_head.lhead = dentry;
        }
}

static void
dir_release_entries(dentry_t *dentry)
{
        dentry_t *child = dentry->children;
        for (; child; child = child->next_sib) {
                hash_key_t key = {.key32 = child->path};
                ht_remove(dentry->table, key);
                _dir_release(child);
        }
}

void
dir_remove_child(dentry_t *dir, dentry_t *entry)
{
        if (entry->prev_sib)
                entry->prev_sib->next_sib = entry->next_sib;
        else
                dir->children = entry->next_sib;
        entry->prev_sib = NULL;

        if (entry->next_sib)
                entry->next_sib->prev_sib = entry->prev_sib;
        entry->next_sib = NULL;

        _dir_release(entry->parent);
        entry->parent = NULL;
        _dir_release(entry);

        hash_key_t key = {.key32 = entry->path};
        if (ht_remove(dir_head.table, key)) {
                printf("[DENTRY] dentry can't free'd in hash table\n");
                abort();
        }

        kfree(entry->mutex);
        kfree(entry->path);

        if (entry->valid) 
                dir_release_entries(entry);

        ht_free(entry->table);
        kfree(entry);
}

void
dir_unlock(dentry_t *dentry)
{
        mutex_acquire(dir_head.mutex);
        mutex_release(dentry->mutex);
        mutex_release(dir_head.mutex);
}

static void
dir_evict(void)
{
        if (!dir_head.ltail) {
                printf("[DENTRY] there aren't any free dentries\n");
                abort();
        }

        if (dir_head.ltail->ref_count) {
                printf("[DENTRY] dentry shouldn't be already free\n");
                abort();
        }

        dentry_t *old = dir_head.ltail;

        hash_key_t key = {.key32 = old->path};
        if (ht_remove(dir_head.table, key)) {
                printf("[DENTRY] dentry can't free'd in hash table\n");
                abort();
        }

        dir_remove(old);
        kfree(old->mutex);
        kfree(old->path);

        if (old->valid) 
                dir_release_entries(old);

        ht_free(old->table);
        kfree(old);
}

void
dir_release(dentry_t *dentry)
{
        mutex_acquire(dir_head.mutex);
        _dir_release(dentry);
        mutex_release(dir_head.mutex);
}

static dentry_t*
dir_iter_get(char *path, size_t len)
{
        size_t orig_len = len;
        dentry_t *dentry;
        while (len) {
                for (; path[len] != '/' && len; --len);
                path[len] = 0;
                
                hash_key_t key = {.key32 = path};
                dentry = ht_get(dir_head.table, key);
                if (dentry)
                        break;
        }

        if (!dentry)
                return NULL;

        int (*parse_dir)(dentry_t *) = dir_head.dir_itfs[0].parse_dir;
        while (len != orig_len) {

                mutex_release(dir_head.mutex);
                parse_dir(dentry);
                mutex_acquire(dir_head.mutex);

                path[len] = '/';
                for (; len != orig_len && path[len]; ++len);
                
                if (len > 1 && path[len - 1] == '.') {
                    if (len > 2 && path[len - 2] == '.') {
                        dentry = dentry->parent;
                        continue;
                    }

                    continue;
                }

                if (orig_len > 1 && path[orig_len - 1] == '/') {
                    path[orig_len - 1] = 0;
                }
                hash_key_t key = {.key32 = path};
                dentry = ht_get(dir_head.table, key);
                if (!dentry) {
                        printf("[DENTRY] not existing dentry\n");
                        return NULL;
                }
        }

        return dentry;
}

void
dir_lock(dentry_t *dentry)
{
        mutex_acquire(dir_head.mutex);
        mutex_acquire(dentry->mutex);
        mutex_release(dir_head.mutex);
}

char*
dir_sum_path(void *path1, void *path2, size_t len1, size_t len2)
{
        char *new_path = kmalloc(len1 + len2 + 2);
        
        int off = len1;
        memcpy(new_path, path1, len1);
        if (new_path[off - 1] != '/')
                new_path[off++] = '/';

        memcpy(new_path + off, path2, len2);
        new_path[off + len2] = 0;

        return new_path;       
}

dentry_t*
dir_get(char *path)
{
        mutex_acquire(dir_head.mutex);

        int new_path_flag = 0;
        if (*path != '/') {
                dentry_t *cur = current_task->current_dir;
                path = dir_sum_path(cur->path, path, strlen(cur->path), strlen(path));
                new_path_flag = 1;
        }
 
        size_t len = strlen(path);
        if (len > 1 && path[len - 1] == '/') path[len - 1] = '\0';
 
        hash_key_t key = {.key32 = path};
        dentry_t *dentry = ht_get(dir_head.table, key);

        if (dentry) {
                if (!dentry->ref_count++)
                        dir_remove(dentry);

                mutex_release(dir_head.mutex);
                if (new_path_flag)
                        kfree(path);

                return dentry;
        }

        char path_buf[len + 1];
        memcpy(path_buf, path, len + 1);
        dentry = dir_iter_get(path_buf, len);
        
        mutex_release(dir_head.mutex);
        if (new_path_flag)
                kfree(path);

        return dentry;
}

dentry_t*
dir_set(int device, char *path, uint32_t inode_n, uint32_t offset)
{
        mutex_acquire(dir_head.mutex);
        
        inode_t *inode = inode_get(device, inode_n);
        dentry_t *dentry = dentry_alloc(path, inode, offset);

        hash_key_t key = {.key32 = path};
        if (!ht_set(dir_head.table, key, dentry)) {
                mutex_release(dir_head.mutex);
                return dentry;
        }
        
        int ret;
        do {
                dir_evict();
                ret = ht_set(dir_head.table, key, dentry);
        } while (ret && dir_head.ltail);

        if (!ret) {
                mutex_release(dir_head.mutex);
                return dentry;
        }
        
        printf("[DENTRY] can't get any free dentry\n");
        abort();
}

static dentry_t*
dir_get_parent(char *path, char **name)
{
        size_t len = strlen(path);
        char *tmp = kmalloc(len + 1);
        memcpy(tmp, path, len + 1);
        for (; tmp[len] != '/' && len; --len);
        tmp[len] = 0;

        dentry_t *dentry = dir_get(tmp);
        if (name)
                *name = &path[len + 1];
        
        kfree(tmp);
        return dentry;
}

int
dir_link(char *oldpath, char *newpath)
{
        dentry_t *dentry = dir_get(oldpath); 
        if (!dentry)
                return -1;

        dir_lock(dentry);
        inode_t *inode = dentry->inode;
        inode_lock(inode);
        
        if (~inode->mode & EXT2_TYPE_FILE) {
                inode_unlock(inode);
                inode_release(inode);

                dir_unlock(dentry);
                dir_release(dentry);
                return -1;
        }

        char *name = NULL;
        dentry_t *dst_dentry = dir_get_parent(newpath, &name);
        if (!dst_dentry) {
                inode_unlock(inode);
                inode_release(inode);

                dir_unlock(dentry);
                dir_release(dentry);
                return -1;
        }

        dir_lock(dst_dentry);
        inode_t *dst = dst_dentry->inode; 
        inode_lock(dst);

        int ret = -1;
        int (*write)(dentry_t *, dentry_t *, char *, int);
        write = dir_head.dir_itfs[dst->device].write_dir;
        if (dst->device == inode->device && !write(dst_dentry, dentry, name, 1)) {
                ++inode->hard_links_count;
                inode_update(inode);
                ret = 0;
        }

        inode_unlock(dst);
        inode_unlock(inode);

        dir_unlock(dst_dentry);
        dir_release(dst_dentry);
        dir_unlock(dentry);
        dir_release(dentry);
        return ret;
}

int
dir_unlink(char *path)
{
        char *name = NULL;
        dentry_t *dir_dentry = dir_get_parent(path, &name);
        if (!dir_dentry)
                return -1;
        if (!strcmp(name, ".") || !strcmp(name, ".."))
                return -1;
         
        dentry_t *entry_dentry = dir_get(path); 
        if (!entry_dentry) {
                dir_release(dir_dentry);
                return -1;
        }
       
        inode_t *dir = dir_dentry->inode;
        dir_lock(dir_dentry);
        inode_lock(dir);

        inode_t *entry = entry_dentry->inode;
        dir_lock(entry_dentry);
        inode_lock(entry);

        int ret = -1;
        int (*write_dir)(dentry_t *, dentry_t *, char *, int);
        write_dir = dir_head.dir_itfs[dir->device].write_dir;
        if (!write_dir(dir_dentry, entry_dentry, NULL, 0)) {
                if (entry->mode & EXT2_TYPE_DIR)
                        --entry->hard_links_count;

                --entry->hard_links_count;
                inode_update(entry);
                ret = 0;
        }
        
        dir_unlock(dir_dentry);
        dir_release(dir_dentry);

        dir_unlock(entry_dentry);
        dir_release(entry_dentry);

        inode_unlock(dir);
        inode_unlock(entry);
        return ret;
}

static void
dir_create_dot(dentry_t *dir, dentry_t *entry)
{
        char *tmp = entry->path;
        size_t len = strlen(entry->path);
        size_t max_len = len;
        for (; tmp[len] != '/' && len; --len);
        if (len > max_len - 2)
                ++max_len;
        char *tmp1 = kmalloc(max_len);
        memcpy(tmp1, entry->path, len);
        tmp1[len + 1] = '.';
        tmp1[len + 2] = 0;
        
        dentry_t *sdot = dir_set(entry->inode->device, tmp1, entry->inode->n, 0);
        dir_lock(sdot);

        int (*write)(dentry_t *, dentry_t *, char *, int);
        write = dir_head.dir_itfs[entry->inode->device].write_dir;
        if (write(entry, sdot, ".", 1)) {
                printf("[DENTRY] can't link up '.'\n");
                abort();
        }

        if (len > max_len - 3)
                max_len += 2;
        char *tmp2 = kmalloc(max_len);
        memcpy(tmp1, entry->path, len);
        tmp[len + 1] = '.';
        tmp[len + 2] = '.';
        tmp[len + 3] = 0;
        
        dentry_t *ddot = dir_set(entry->inode->device, tmp2, dir->inode->n, 0);
        dir_lock(ddot);

        write = dir_head.dir_itfs[entry->inode->device].write_dir;
        if (write(entry, ddot, "..", 1)) {
                printf("[DENTRY] can't link up '..'\n");
                abort();
        }

        dir_unlock(sdot);
        dir_unlock(ddot);
        dir_release(sdot);
        dir_release(sdot);
}

dentry_t*
dir_create(char *path, uint16_t mode, int device)
{
        inode_t *inode = dinode_alloc(device, mode);
        dentry_t *entry = dir_set(device, path, inode->n, 0);
        
        char *name = NULL;
        dentry_t *dir = dir_get_parent(path, &name);

        dir_lock(entry);
        dir_lock(dir);

        inode_lock(entry->inode);
        inode_lock(dir->inode);
        
        int (*write)(dentry_t *, dentry_t *, char *, int);
        write = dir_head.dir_itfs[device].write_dir;
        if (write(dir, entry, name, 1)) {
                printf("[DENTRY] can't link up newly created entry\n");
                abort();
        }
        
        if (mode & EXT2_TYPE_DIR) {
                ++inode->hard_links_count;
                inode_update(inode);

                dir_create_dot(dir, entry);
        }

        dir_unlock(entry);
        dir_unlock(dir);
        dir_release(dir);
        
        inode_unlock(entry->inode);
        inode_unlock(dir->inode);

        return entry;
}

int
dir_make(char *path, int mode)
{
        dentry_t *dentry = dir_create(path, EXT2_TYPE_DIR | (mode & 0xFFF), 0);
        if (!dentry)
                return -1;
        
        dir_release(dentry);
        return 0;
}

int
dir_change(char *path)
{
        dentry_t *dentry = dir_get(path);
        if (!dentry)
                return -1;

        inode_t *inode = dentry->inode;

        inode_lock(inode);
        if (~inode->mode & EXT2_TYPE_DIR) {
                inode_unlock(inode);
                return -1;
        }
        inode_unlock(inode);

        dir_release(current_task->current_dir);
        current_task->current_dir = dentry;

        return 0;
}

void
dir_debug_print(void)
{
        kprintf("[DENTRY] hash table %x:\n", dir_head.table->capacity);
        hash_entry_t *entry = dir_head.table->entries;
        kprintf("index | key | value\n");
        for (size_t i = 0; i < dir_head.table->capacity; ++i) {
                if (entry[i].state == HT_VALID) {
                        dentry_t *dentry = entry[i].value;
                        kprintf("%d, %d, %d, %s\n", i, dentry->ref_count,
                                dentry->inode->n, entry[i].key32);

                        dentry_t *child = dentry->children;
                        while (child) {
                            
                            kprintf("%s, %d\n", child->path, child->inode->n);

                            child = child->next_sib;
                        }

                        kprintf("\n");
                }
                else if (entry[i].state == HT_TOMBSTONE)
                        kprintf("%d: tombstone\n", i);
        }
        kprintf("\n");
        
        kprintf("[DENTRY] next linked list:\n");
        dentry_t *dentry = dir_head.lhead;
        kprintf("addr | next | prev | inode num\n");
        for (; dentry; dentry = dentry->next)
                kprintf("%x, %x, %x, %x\n", dentry, dentry->next, dentry->prev, dentry->inode->n);
        kprintf("\n");
        
        kprintf("[DENTRY] prev linked list:\n");
        dentry = dir_head.ltail;
        kprintf("addr | next | prev\n");
        for (; dentry; dentry = dentry->prev)
                kprintf("%x, %x, %x\n", dentry, dentry->next, dentry->prev);
        kprintf("\n\n");
}

void dir_add_itf(int device, 
                 int (*parse_dir)(dentry_t *),
                 int (*write_dir)(dentry_t *, dentry_t *, char *, int),
                 void (*parse_root)(int))
{
        dir_head.dir_itfs[device].valid = 1;
        dir_head.dir_itfs[device].parse_dir = parse_dir;
        dir_head.dir_itfs[device].write_dir = write_dir;
        dir_head.dir_itfs[device].parse_root = parse_root;
}

void
dir_init(void)
{
        dir_head.mutex = mutex_create();
        dir_head.lhead = NULL;
        dir_head.ltail = NULL;
        dir_head.table = ht_create(DIR_TABLE_SIZE, DIR_LOAD_FACTOR, HT_PTRKEY);
        
        for (int i = 0; i < 4; ++i) {
                if (!dir_head.dir_itfs[i].valid)
                        continue;

                void (*parse_root)(int);
                parse_root = dir_head.dir_itfs[i].parse_root;
                parse_root(i);
        }

        current_task->current_dir = dir_get("/");
}
