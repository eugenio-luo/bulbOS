#ifndef _KERNEL_DIR_H
#define _KERNEL_DIR_H

#include <kernel/inode.h>

typedef struct dentry {
        /* HEAD */
        int valid;
        int ref_count;
        uint32_t offset; /* offset in dir inode buffer */
        struct inode *inode;
        struct semaphore *mutex;
        struct dentry *next;
        struct dentry *prev;

        char *path;

        /* TAIL */
        struct dentry *next_sib;
        struct dentry *prev_sib;
        struct dentry *parent;
        struct dentry *children; /* only if dentry is dir */ 
        hash_table_t *table;
} dentry_t;

#define DIR_TABLE_SIZE    50
#define DIR_LOAD_FACTOR   75

void dir_init(void);
char* dir_sum_path(void *path1, void *path2, size_t len1, size_t len2);
void dir_add_child(dentry_t *dir, dentry_t *entry);
void dir_remove_child(dentry_t *dir, dentry_t *entry);
dentry_t* dir_get(char *path);
dentry_t* dir_dup(dentry_t *dentry);
void dir_lock(dentry_t *dentry);
void dir_release(dentry_t *dentry);
void dir_unlock(dentry_t *dentry);
dentry_t* dir_set(int device, char *path, uint32_t inode_n, uint32_t offset);
int dir_link(char *oldpath, char *newpath);
int dir_unlink(char *path);
dentry_t* dir_create(char *path, uint16_t mode, int device);
int dir_make(char *path, int mode);
int dir_change(char *path);
void dir_add_itf(int device, 
                 int (*parse_dir)(dentry_t *),
                 int (*write_dir)(dentry_t *, dentry_t *, char *, int),
                 void (*parse_root)(int));

void dir_debug_print(void);

#endif
