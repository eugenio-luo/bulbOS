#ifndef _KERNEL_FILESYSTEM_H
#define _KERNEL_FILESYSTEM_H

typedef struct {
        uint32_t block;
        uint32_t idx;
} inode_idx_t;

typedef struct {
        uint32_t total_inodes;
        uint32_t total_blocks;
        uint32_t reserved_blocks;
        uint32_t free_blocks;
        uint32_t free_inodes;
        uint32_t first_data_block;
        uint32_t log_block_size;
        uint32_t log_frag_size;
        uint32_t blocks_pg;
        uint32_t frags_pg;
        uint32_t inodes_per_group;
        uint32_t last_mount_time;
        uint32_t last_write_time;
        uint16_t mount_count;
        uint16_t max_mount_time;
        uint16_t signature;
        uint16_t state;
        uint16_t errors;
        uint16_t minor_rev;
        uint32_t last_check_time;
        uint32_t check_interval;
        uint32_t creator_os;
        uint32_t major_rev;
        uint16_t res_user_id;
        uint16_t res_group_id;
        
        /* use these field only if major_rev > 0 */
        uint32_t first_inode;
        uint16_t inode_size;
        uint16_t backup_group;
        uint32_t optional_feature;
        uint32_t required_feature;
        uint32_t readonly_feature;
        uint8_t  file_system_id[16];
        uint8_t  volume_name[16];
        uint8_t  last_path[64];
        uint32_t bitmap_algo;
        uint8_t  prealloc_file_blocks;
        uint8_t  prealloc_dir_blocks;
        uint16_t unused_0;
        uint8_t  journal_id[16];
        uint32_t journal_inode;
        uint32_t journal_device;
        uint32_t first_orphan;
        uint32_t hash_seeds[4];
        uint8_t  default_hash;
        uint8_t  unused_1[3];
        uint32_t default_mount;
        uint32_t first_meta_block;
        uint8_t  unused_2[760];
} __attribute__ ((packed)) ext2_sblock_t;

typedef struct {
        uint32_t block_bitmap;
        uint32_t inode_bitmap;
        uint32_t inode_table;
        uint16_t free_blocks;
        uint16_t free_inodes;
        uint16_t directories;
        uint8_t  unused[14];
} __attribute__ ((packed)) ext2_bgd_t;

#define INODE_BLOCKS_COUNT   15

typedef struct {
        uint16_t mode;
        uint16_t user_id;
        uint32_t size;
        uint32_t last_access_time;
        uint32_t creation_time;
        uint32_t last_modify_time;
        uint32_t deletion_time;
        uint16_t group_id;
        uint16_t hard_links_count;
        uint32_t sectors;
        uint32_t flags;
        uint32_t os_specific_1;
        uint32_t blocks[INODE_BLOCKS_COUNT];
        uint32_t gen_num;
        uint32_t file_acl;
        uint32_t dir_acl;
        uint32_t frag_addr;
        uint8_t  os_specific_2[12];
} __attribute__ ((packed)) ext2_inode_t;

typedef enum {
        EXT2_TYPE_FIFO = 0x1000,
        EXT2_TYPE_DIR  = 0x4000,
        EXT2_TYPE_FILE = 0x8000,
} ext2_mode_t;

typedef struct {
        uint32_t inode;
        uint16_t size;
        uint8_t  length;
        uint8_t  type;
        uint8_t  name[]; /* name[length], size is variable */
} __attribute__ ((packed)) ext2_dir_t;

#define DIR_ENTRY_SIZE(length)         (sizeof(ext2_dir_t) + length)
#define DIR_ENTRY_MAX_SIZE             CALC_DIR_ENTRY_SIZE(255)

typedef enum {
        FILE_TYPE_UNKNOWN = 0,
        FILE_TYPE_FILE,
        FILE_TYPE_DIR,
        FILE_TYPE_CHARDEV,
        FILE_TYPE_BDEV,
        FILE_TYPE_FIFO,
        FILE_TYPE_SOCKET,
        FILE_TYPE_SLINK,
} file_type_t;

#define EXT2_SIGNATURE     0xEF53
#define LOG_HEADER_SIGN    0x20040605
#define SUPERBLOCK_START   1024
#define BLOCK_SIZE         (1024 << sblock->log_block_size)     
#define BGD_PER_BLOCK      (BLOCK_SIZE / sizeof(ext2_bgd_t))
#define INODES_PER_BLOCK   (BLOCK_SIZE / sblock->inode_size)
#define EXT2_ROOT_INODE    2
#define BG_DESCR_START     (SUPERBLOCK_START + sizeof(ext2_sblock_t))
#define PTR_ENTRY_BLOCKS   (1024 / 4)
#define EMPTY_DIR_SIZE     (0xC * 2) /* only two entries: '.' and '..' */

#define DIRECT_BLOCKS      12
#define IND1_IDX           12
#define IND2_IDX           13
#define IND3_IDX           14

#define IND1_PTR_BLOCKS    PTR_ENTRY_BLOCKS
#define IND2_PTR_BLOCKS    (IND1_PTR_BLOCKS * PTR_ENTRY_BLOCKS)
#define IND3_PTR_BLOCKS    (IND2_PTR_BLOCKS * PTR_ENTRY_BLOCKS)
#define MAX_FILE_SIZE      ((IND3_PTR_BLOCKS + IND2_PTR_BLOCKS + IND1_PTR_BLOCKS + DIRECT_BLOCKS) * 1024U) 

#define IND1_LIMIT         (DIRECT_BLOCKS + IND1_PTR_BLOCKS)
#define IND2_LIMIT         (IND1_LIMIT + IND2_PTR_BLOCKS)
#define IND3_LIMIT         (IND2_LIMIT + IND3_PTR_BLOCKS)

#define RUP_DIVISION(X, Y)   ((X + (Y - 1)) / Y)

void ext2_init(int device);

#endif
