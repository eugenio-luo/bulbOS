#ifndef _KERNEL_INODE_H
#define _KERNEL_INODE_H

#include <stdint.h>

#include <utils/hashtable.h>
#include <kernel/mutex.h>
#include <kernel/filesystem.h>
#include <kernel/file.h>
#include <kernel/dir.h>

typedef struct stat stat_t;

typedef struct inode {
        int valid;
        int ref_count;
        int device;
        uint32_t n;
        struct semaphore *mutex;
       
        uint16_t mode;
        uint16_t hard_links_count;
        uint32_t size;
        uint32_t last_access_time;
        uint32_t creation_time;
        uint32_t last_modify_time;
        uint32_t deletion_time;
        uint32_t sectors;
        uint32_t blocks[INODE_BLOCKS_COUNT];

        int (*write)(struct inode *, void *, size_t, size_t);
        int (*read)(struct inode *, void *, size_t, size_t);
        /*
        union {
                int (*write)(struct inode *, void *, size_t, size_t);
                int (*write_dir)(struct dentry *, struct dentry *, char *, int);
        };
        union {
                int (*read)(struct inode *, void *, size_t, size_t);
                int (*read_dir)(struct dentry *);
        };
        */
                
        struct inode *next;
        struct inode *prev;
} inode_t;

#define INODE_TABLE_SIZE    50
#define INODE_LOAD_FACTOR   75

void inode_init(void);
void inode_add_itf(int device, 
                   void (*inode_load)(inode_t *),
                   void (*inode_update)(inode_t *),
                   void (*inode_trunc)(inode_t *),
                   uint32_t (*dinode_alloc)(int));
inode_t* inode_dup(inode_t* inode);
inode_t *inode_get(int device, uint32_t inode_n);
inode_t *dinode_alloc(int device, uint16_t);
void inode_lock(inode_t *inode);
void inode_unlock(inode_t *inode);
void inode_release(inode_t *inode);
void inode_update(inode_t *inode);
void inode_trunc(inode_t *inode);

#endif
