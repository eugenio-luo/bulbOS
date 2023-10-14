#ifndef _KERNEL_PAGING_H
#define _KERNEL_PAGING_H

#include <stdint.h>

#include <kernel/memory.h>

typedef uintptr_t page_entry_t;
typedef uintptr_t pd_entry_t;

#define NUM_OF_ENTRIES 1024
#define PAGE_MEMORY    (NUM_OF_ENTRIES * PAGE_FRAME_SIZE) /* size of entire page of memory */
/* useful macro for recursively accessing page tables and page directory */
#define RECURSIVE_INDEX(index) (PDE_LAST_INDEX_START + index * PAGE_FRAME_SIZE)

typedef enum {
        PD_PRESENT =        (1 << 0),
        PD_READ_WRITE =     (1 << 1), 
        PD_USER =           (1 << 2), /* user/supervisor */
        PD_PWT =            (1 << 3), /* write-through */
        PD_PCD =            (1 << 4), /* cache disabled */
        PD_ACCESSED =       (1 << 5), 
        PD_PAGESIZE =       (1 << 7), /* page size, NEVER SET THIS */
} pd_flags_t;

typedef enum {
        PT_PRESENT =        (1 << 0),
        PT_READ_WRITE =     (1 << 1), 
        PT_USER =           (1 << 2), /* user/supervisor */
        PT_PWT =            (1 << 3), /* write-through */
        PT_PCD =            (1 << 4), /* cache disabled */
        PT_ACCESSED =       (1 << 5), 
        PT_DIRTY =          (1 << 6),
        PT_PAT =            (1 << 7), /* page attribute table, should be 0 */
        PT_GLOBAL =         (1 << 8),
} pt_flags_t;

typedef enum {
        NO_ALLOC = 0,
        PAGE_ALLOC,
} alloc_flag_t;

extern pd_entry_t *page_directory;

void page_init(void);

void page_identity_map(pd_entry_t *page_dir, uintptr_t start_addr, uintptr_t size);
page_entry_t *page_get_pt(pd_entry_t *page_dir, uintptr_t addr, alloc_flag_t alloc_flag);
page_entry_t *page_get_pt_entry(pd_entry_t *page_dir, uintptr_t addr, alloc_flag_t alloc_flag);
uintptr_t page_get_phys_addr(pd_entry_t *page_dir, uintptr_t addr);
void pd_add_table(pd_entry_t *pd_entry, page_entry_t *page_entry, pd_flags_t flags);
void pt_add_entry(page_entry_t *pt_entry, void *page, pt_flags_t flags);

static inline uint32_t
page_get_pt_index(uintptr_t addr)
{
        return addr >> 12 & 0x3FF;
}

static inline uint32_t
page_get_pd_index(uintptr_t addr)
{
        return addr >> 22;
}

#endif

