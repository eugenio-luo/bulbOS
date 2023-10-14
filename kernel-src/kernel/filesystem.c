#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <utils/hashtable.h>

#include <kernel/filesystem.h>
#include <kernel/bio.h>
#include <kernel/vmm.h>
#include <kernel/inode.h>
#include <kernel/dir.h>

static ext2_sblock_t *sblock;
static ext2_bgd_t *bg_descrs;

static inode_idx_t
ext2_inode_idx(uint32_t inode_n)
{
        uint32_t inode_idx = inode_n - 1;

        uint32_t descr_idx = inode_idx / sblock->inodes_per_group;
        uint32_t descr_inode_idx = inode_idx % sblock->inodes_per_group;

        uint32_t inode_table_idx = bg_descrs[descr_idx].inode_table;
        uint32_t table_block_idx = descr_inode_idx / INODES_PER_BLOCK;
       
        return (inode_idx_t) {
                .block = inode_table_idx + table_block_idx,
                .idx = descr_inode_idx % INODES_PER_BLOCK,
        };
}

bio_buf_t*
ext2_read_block(int device, uint32_t block)
{
        return bio_read(device, block, BLOCK_SIZE);
}

ext2_inode_t*
ext2_read_inode(bio_buf_t **buf, int device, uint32_t inode_n)
{
        inode_idx_t inode_idx = ext2_inode_idx(inode_n);
        *buf = bio_read(device, inode_idx.block, BLOCK_SIZE);
        ext2_inode_t *inode = (ext2_inode_t*) (*buf)->buffer + inode_idx.idx; 
        return inode;
}

void
ext2_write_block(bio_buf_t *buf)
{
        bio_write(buf);
}

static void
ext2_zero_block(int device, uint32_t block)
{
        bio_buf_t *buf = ext2_read_block(device, block);
        memset(buf->buffer, 0, BLOCK_SIZE);
        ext2_write_block(buf);
        bio_release(buf);
}

static uint32_t
ext2_alloc(int device, int is_inode)
{
        static uint32_t last_allocs[8] = {0, 0, 0, 0, 11, 11, 11, 11};

        uint32_t tot_blocks = sblock->total_blocks;
        uint32_t blocks_pg = sblock->blocks_pg;

        uint32_t i = last_allocs[device + is_inode * 4];
        ext2_bgd_t *bg_descr = bg_descrs + i;

        do {
                uint32_t bitmap_idx = (is_inode) ?
                        bg_descr->inode_bitmap : bg_descr->block_bitmap;

                bio_buf_t *bitmap = ext2_read_block(device, bitmap_idx);

                for (uint32_t j = i; j < blocks_pg && i + j < tot_blocks; ++j) {
                        uint8_t bit = 1 << (j % 8);
                        if (bitmap->buffer[j / 8] & bit)
                                continue;

                        bitmap->buffer[j / 8] |= bit;
                        ext2_write_block(bitmap);
                        bio_release(bitmap);

                        ext2_zero_block(device, i + j);
                        
                        return i + j;
                }

                bio_release(bitmap);
                
                i = ((i / blocks_pg + 1) * blocks_pg) % tot_blocks;
                bg_descr = &bg_descrs[i];
        } while (i != last_allocs[device]);

        printf("[FS] can't alloc a block\n");
        abort();
}

static uint32_t
ext2_block_alloc(int device)
{
        return ext2_alloc(device, 0);
}

static uint32_t
ext2_inode_alloc(int device)
{
        return ext2_alloc(device, 1);
}

static void
ext2_free(int device, uint32_t n, int is_inode)
{
        uint32_t blocks_pg = sblock->blocks_pg;
        ext2_bgd_t *bg_descr = bg_descrs + (n / blocks_pg);

        uint32_t bitmap_idx = (is_inode) ?
                bg_descr->inode_bitmap : bg_descr->block_bitmap;
        bio_buf_t *bitmap = ext2_read_block(device, bitmap_idx);

        uint32_t idx = n % blocks_pg;
        uint32_t bit = 1 << (idx % 8);
        if (~bitmap->buffer[idx / 8] & bit) {
                printf("[FS] freeing free block\n");
                abort();
        }

        bitmap->buffer[idx / 8] &= ~bit;
        ext2_write_block(bitmap);
        bio_release(bitmap);
}

static void
ext2_block_free(int device, uint32_t block)
{
        ext2_free(device, block, 0);
}

static void
ext2_inode_free(int device, uint32_t inode)
{
        ext2_free(device, inode, 1);
}

static uint32_t
ext2_get_div(int times)
{
        switch (times) {
        case 0:
                return 1;
        case 1:
                return IND1_PTR_BLOCKS;
        case 2:
                return IND2_PTR_BLOCKS;
        default:
                abort();
        }

        return -1;
}

static uint32_t
ext2_indblock(int device, void *ind, uint32_t idx, int times)
{
        uint32_t div = ext2_get_div(times);
        uint32_t r_idx = idx / div, n_idx = idx % div;
        
        uint32_t *ind32 = (uint32_t*) ind;
        if (!ind32[r_idx])
                ind32[r_idx] = ext2_block_alloc(device);

        bio_buf_t *buf = ext2_read_block(device, ind32[r_idx]);
        uint32_t *block = (uint32_t*) buf->buffer;
        if (!block)
                block[n_idx] = ext2_block_alloc(device);

        uint32_t ret = block[n_idx];
        if (times)
                ret = ext2_indblock(device, block, n_idx, --times);

        ext2_write_block(buf);
        bio_release(buf);
        return ret;
}
     
static uint32_t
ext2_get_iblock(inode_t *inode, uint32_t idx)
{
        if (idx < DIRECT_BLOCKS) {
                if (!inode->blocks[idx])
                        inode->blocks[idx] = ext2_block_alloc(inode->device);

                uint32_t block = inode->blocks[idx];
                return block;
        }

        uint32_t ind_idx, times, r_idx;
        if (idx < IND1_LIMIT) {
                
                ind_idx = IND1_IDX;
                times = 0;
                r_idx = idx - DIRECT_BLOCKS;

        } else if (idx < IND2_LIMIT) {

                ind_idx = IND2_IDX;
                times = 1;
                r_idx = idx - IND1_LIMIT;

        } else if (idx < IND3_LIMIT) {

                ind_idx = IND3_IDX;
                times = 2;
                r_idx = idx - IND2_LIMIT;

        } else {
                abort();
        }

        if (!inode->blocks[ind_idx])
                inode->blocks[ind_idx] = ext2_block_alloc(inode->device);

        bio_buf_t *buf = ext2_read_block(inode->device, inode->blocks[ind_idx]);
        uint32_t block = ext2_indblock(inode->device, buf->buffer, r_idx, times);
        
        ext2_write_block(buf);
        bio_release(buf);
        return block;
}

void
ext2_inode_update(inode_t *inode)
{
        bio_buf_t *buf = NULL;
        ext2_inode_t *d_inode = ext2_read_inode(&buf, inode->device, inode->n);

        d_inode->mode = inode->mode;
        d_inode->size = inode->size;
        d_inode->last_access_time = inode->last_access_time;
        d_inode->creation_time = inode->creation_time;
        d_inode->last_modify_time = inode->last_modify_time;
        d_inode->deletion_time = inode->deletion_time;
        d_inode->hard_links_count = inode->hard_links_count;
        d_inode->sectors = inode->sectors;

        for (int i = 0; i < INODE_BLOCKS_COUNT; ++i)
                d_inode->blocks[i] = inode->blocks[i];

        ext2_write_block(buf);
        bio_release(buf);
}
        
static void
ext2_ind_trunc(int device, void *block, int times)
{
        uint32_t *block32 = (uint32_t*) block;
        
        for (int i = 0; i < PTR_ENTRY_BLOCKS; ++i) {
                if (!block32[i])
                        break;

                if (times) {
                        bio_buf_t *buf = ext2_read_block(device, block32[i]);
                        ext2_ind_trunc(device, buf->buffer, --times);
                        ext2_write_block(buf);
                        bio_release(buf);
                } else {
                        ext2_block_free(device, block32[i]);
                        block32[i] = 0;
                }
        }
}

static void
ext2_truncate(inode_t *inode) {

        for (int i = 0; i < DIRECT_BLOCKS; ++i) {
                if (!inode->blocks[i])
                        return;

                ext2_block_free(inode->device, inode->blocks[i]);
                inode->blocks[i] = 0;
        }

        for (int idx = IND1_IDX, times = 0; idx <= IND3_IDX; ++idx, ++times) {
                if (!inode->blocks[idx])
                        return;
        
                bio_buf_t *buf = ext2_read_block(inode->device, inode->blocks[idx]);
                ext2_ind_trunc(inode->device, buf->buffer, times);
                ext2_write_block(buf);
                bio_release(buf);
        }

        inode->size = 0;
        ext2_inode_update(inode);
}

int
ext2_read_content(inode_t *inode, void *dst, size_t offset, size_t size)
{
        if (offset > inode->size)
                return 0;

        if (offset + size > inode->size)
                size = inode->size - offset;

        uint8_t *dst8 = (uint8_t*) dst;
        
        size_t i = 0, len;
        for (; i < size; i += len, offset += len, dst8 += len) {
                uint32_t block_idx = ext2_get_iblock(inode, offset / BLOCK_SIZE);
                bio_buf_t *buf = ext2_read_block(inode->device, block_idx);

                size_t buf_off = offset % BLOCK_SIZE;
                len = (size - i < 1024 - buf_off) ? (size - i) : (1024 - buf_off);
                memcpy(dst8, buf->buffer + buf_off, len);

                bio_release(buf);
        }

        return i;
}

int
ext2_write_content(inode_t *inode, void *src, size_t offset, size_t size)
{
        if (offset > inode->size)
                return -1;

        if (offset + size > MAX_FILE_SIZE) 
                return -1;

        uint8_t *src8 = (uint8_t*) src;
        
        size_t i = 0, len;
        for (; i < size; i += len, offset += len, src8 += len) {
                uint32_t block_idx = ext2_get_iblock(inode, offset / BLOCK_SIZE);
                bio_buf_t *buf = ext2_read_block(inode->device, block_idx);

                size_t buf_off = offset % BLOCK_SIZE;
                len = (size - i < 1024 - buf_off) ? (size - i) : (1024 - buf_off);
                memcpy(buf->buffer + buf_off, src8, len);

                ext2_write_block(buf);
                bio_release(buf);
        }

        if (offset > inode->size)
                inode->size = offset;
        
        ext2_inode_update(inode);
        return i;
}

static char*
ext2_sum_path(void *path1, void *path2, size_t len1, size_t len2)
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

static int
ext2_unlink(dentry_t *dir, dentry_t *entry)
{
        inode_t *dst = dir->inode;
        inode_t *ientry = entry->inode; 
        
        if (!ientry->hard_links_count) {
                printf("[DENTRY] inode don't have any links\n");
                abort();
        }
        
        if (ientry->mode & EXT2_TYPE_DIR && ientry->size != EMPTY_DIR_SIZE)
                return -1;
        
        uint8_t *buffer = kmalloc(dst->size);
        ext2_read_content(dst, buffer, 0, dst->size);

        ext2_dir_t *disk_entry = (ext2_dir_t *)(buffer + entry->offset);
        size_t new_offset = entry->offset;
        memset(disk_entry, 0, disk_entry->size);

        dentry_t *rem_entry = entry;
        entry = rem_entry->prev_sib;
        ext2_dir_t *prev_entry = (ext2_dir_t*)(buffer + rem_entry->next_sib->offset);
        size_t act_offset = rem_entry->next_sib->offset;
        dir_remove_child(dir, rem_entry);

        while (entry) {
                disk_entry = (ext2_dir_t *)(buffer + act_offset);
                size_t rem = BLOCK_SIZE - (new_offset % BLOCK_SIZE);

                if (disk_entry->size < rem) {
                        prev_entry->size = new_offset - entry->next_sib->offset;
                } else {
                        prev_entry->size = rem;
                        new_offset = (new_offset / BLOCK_SIZE + 1) * BLOCK_SIZE;
                }
                entry->offset = new_offset;
                memmove(buffer + new_offset, buffer + act_offset, disk_entry->size);

                new_offset = ALIGN_ADDR(entry->offset, 4);
                prev_entry = disk_entry;
                entry = entry->next_sib;
                act_offset = entry->offset;
        }

        prev_entry->size = BLOCK_SIZE - (act_offset % BLOCK_SIZE);
        
        ext2_write_content(dst, buffer, 0, dst->size);
        dst->size = (new_offset / BLOCK_SIZE + 1) * BLOCK_SIZE;
        ext2_inode_update(dst);

        return 0;
}

static int
ext2_link(dentry_t *dir, dentry_t *entry, char *name)
{
        inode_t *dst = dir->inode;
        uint8_t *buffer = kmalloc(dst->size);
        ext2_read_content(dst, buffer, 0, dst->size);

        ext2_dir_t *last = NULL;
        for (size_t i = 0; i < dst->size; i += last->size)
                last = (ext2_dir_t *)(buffer + i);

        size_t last_size = (last) ? last->size : 0;
        size_t last_len = (last) ? last->length : 0;
        
        /* TODO: better way to deal with this? */
        size_t last_start = dst->size - last_size;
        size_t last_new_size = DIR_ENTRY_SIZE(last_len); 
        size_t new_start = ALIGN_ADDR(last_start + last_new_size, 4);
        size_t rem = BLOCK_SIZE - (new_start % BLOCK_SIZE);

        size_t len = strlen(name);
        size_t new_size = DIR_ENTRY_SIZE(len);
        if (new_size > rem) {
                /* go to next block */
                new_start = (new_start / BLOCK_SIZE + 1) * BLOCK_SIZE;
                rem = BLOCK_SIZE - new_size;
                dst->size += BLOCK_SIZE;
        } else if (last) {
                last->size = ALIGN_ADDR(last_new_size, 4);
                ext2_write_content(dst, last, last_start, last->size);
        }

        ext2_dir_t *new = kmalloc(new_size);
        new->inode = entry->inode->n;
        new->size = rem;
        new->length = len;
        switch (entry->inode->mode) {
        case EXT2_TYPE_FIFO:
                new->type = FILE_TYPE_FIFO;
                break;
        case EXT2_TYPE_FILE:
                new->type = FILE_TYPE_FILE;
                break;
        case EXT2_TYPE_DIR:
                new->type = FILE_TYPE_DIR;
                break;
        default:
                new->type = FILE_TYPE_UNKNOWN;
                break;
        }

        memcpy(&new->name, name, len);
        ext2_write_content(dst, new, new_start, new_size);
        entry->offset = new_start;
        kfree(new);
        ext2_inode_update(dst);

        return 0;
}

static int
ext2_write_dir(dentry_t *dir, dentry_t *entry, char *name, int link)
{
        return (link) ? ext2_link(dir, entry, name) : ext2_unlink(dir, entry);
}

static int
ext2_parse_dir(dentry_t *dir)
{
        dir_lock(dir);
        if (!dir->table)
                dir->table = ht_create(20, 75, HT_RESIZE | HT_PTRKEY);

        inode_t *inode = dir->inode;
        inode_lock(inode);
        
        uint8_t *buffer = kmalloc(inode->size);
        ext2_read_content(inode, buffer, 0, inode->size);

        ext2_dir_t *entry;
        for (size_t i = 0; i < inode->size; i += entry->size) {
                entry = (ext2_dir_t*)(buffer + i);
                
                char *new_path = ext2_sum_path(dir->path, entry->name,
                                               strlen(dir->path), entry->length);
                
                dentry_t *child = dir_set(inode->device, new_path, entry->inode, i);
                dir_add_child(dir, child);

                dir_release(child);
        }
        
        kfree(buffer);
        inode_unlock(inode);
        dir_unlock(dir);
        return 0;
}

static void
ext2_inode_get(inode_t *inode)
{
        bio_buf_t *buf = NULL;
        ext2_inode_t *d_inode = ext2_read_inode(&buf, inode->device, inode->n);

        inode->mode = d_inode->mode;
        inode->size = d_inode->size;
        inode->last_access_time = d_inode->last_access_time;
        inode->creation_time = d_inode->creation_time;
        inode->last_modify_time = d_inode->last_modify_time;
        inode->deletion_time = d_inode->deletion_time;
        inode->hard_links_count = d_inode->hard_links_count;
        inode->sectors = d_inode->sectors;

        for (int i = 0; i < INODE_BLOCKS_COUNT; ++i)
                inode->blocks[i] = d_inode->blocks[i];

        bio_release(buf);
        
        switch (inode->mode & 0xF000) {
        case EXT2_TYPE_FILE:
                inode->write = ext2_write_content;
                inode->read = ext2_read_content;
                break;
        case EXT2_TYPE_DIR:
                inode->write_dir = ext2_write_dir;
                inode->read_dir = ext2_parse_dir;
        default:
                break;
        }
}

static void
ext2_parse_root(int device)
{
        char *rpath = kmalloc(2);
        rpath[0] = '/', rpath[1] = 0;
        dentry_t *rdentry = dir_set(device, rpath, EXT2_ROOT_INODE, 0);
        
        ext2_parse_dir(rdentry);
        dir_release(rdentry);
}

void
ext2_init(int device)
{
        kprintf("[EXT2] device %d setup STARTING\n", device);

        sblock = kmalloc(sizeof(ext2_sblock_t));
        bio_buf_t *sb_buf = bio_read(device, 1, sizeof(ext2_sblock_t));

        memcpy(sblock, sb_buf->buffer, sizeof(ext2_sblock_t));
        bio_release(sb_buf);

        if (sblock->signature != EXT2_SIGNATURE) {
                kprintf("[EXT2] device %d doesn't have right signature\n");
                return;
        }
        
        uint32_t bgd_count = RUP_DIVISION(sblock->total_blocks, sblock->blocks_pg);
        uint32_t bgd_blocks = RUP_DIVISION(bgd_count, BGD_PER_BLOCK);
        bg_descrs = kmalloc(BLOCK_SIZE * bgd_blocks);

        for (uint32_t i = 0; i < bgd_blocks; ++i) {
                bio_buf_t *buf = bio_read(device, 2 + i, BLOCK_SIZE);
                memcpy(&bg_descrs[i * BGD_PER_BLOCK], buf->buffer, BLOCK_SIZE);
        }

        inode_add_itf(device, ext2_inode_get, ext2_inode_update,
                      ext2_truncate, ext2_inode_alloc);
        dir_add_itf(device, ext2_parse_dir, ext2_write_dir, ext2_parse_root);
        
        kprintf("[EXT2] device %d setup COMPLETE\n", device);
}
