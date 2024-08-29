#include <string.h>
#include <stdio.h>

#include <kernel/debug.h>
#include <kernel/page.h>
#include <kernel/pmm.h>
#include <kernel/memory.h>

pd_entry_t *page_directory;
static page_entry_t page_table1[NUM_OF_ENTRIES] __attribute__ ((aligned(PAGE_FRAME_SIZE))); 

/* these two functions are pratically the same, but they are divided
   because of the debug info */
void
pd_add_table(pd_entry_t *pd_entry, page_entry_t *page_entry, pd_flags_t flags)
{
#ifdef DEBUG
        uintptr_t pd_entry_addr = (uintptr_t)pd_entry;
        uint32_t num_of_entry = (pd_entry_addr & 0xFFF) / 4;
        DPRINTF("[PAGING] page table %x -> entry n.0x%x (%d) of page directory %x\n",
                page_entry, num_of_entry, num_of_entry, pd_entry_addr & 0xFFFFF000);
#endif

        *pd_entry = (uintptr_t)page_entry | flags;
}

void
pt_add_entry(page_entry_t *pt_entry, void *page, pt_flags_t flags)
{
        /*
        uintptr_t pt_entry_addr = (uintptr_t)pt_entry;
        uint32_t num_of_entry = (pt_entry_addr & 0xFFF) / 4;
        DPRINTF("[PAGING] page %x -> entry n.0x%x (%d) of page table %x\n",
                page, num_of_entry, num_of_entry, pt_entry_addr & 0xFFFFF000);
        */
        
        *pt_entry = (uintptr_t)page | flags;
}

/* if page table isn't present and alloc_flag is 1 it get allocated */
page_entry_t*
page_get_pt(pd_entry_t *page_dir, uintptr_t addr, alloc_flag_t alloc_flag)
{
        uint32_t pd_index = page_get_pd_index(addr);
        pd_entry_t *pd_entry = page_dir + pd_index;
        page_entry_t *page_table = (page_entry_t*) RECURSIVE_INDEX(pd_index);
        
        if (~*pd_entry & PD_PRESENT) {
                if (!alloc_flag) {
                        DPRINTF("[ERROR][PAGING] not allocated page 0x%x at index "
                                "0x%x (%d), can't accessed\n",
                                addr, pd_index, pd_index);
                        return NULL;
                }

                /* pd_add_table(pd_entry, pmm_alloc(), PD_PRESENT | PD_READ_WRITE); */
                pd_add_table(pd_entry, pmm_alloc(), PD_PRESENT | PD_READ_WRITE | PD_USER);
                /* memset is IMPORTANT, otherwise there is random data and
                   comparison to check if page table is present can fail */
                memset(page_table, 0, PAGE_FRAME_SIZE);
        }

        return page_table;
}

/* if page isn't present and alloc_flag is 1 it get allocated */
page_entry_t*
page_get_pt_entry(pd_entry_t *page_dir, uintptr_t addr, alloc_flag_t alloc_flag)
{
        uint32_t pt_index = page_get_pt_index(addr);
        page_entry_t *page_entry = page_get_pt(page_dir, addr, PAGE_ALLOC) + pt_index;

        if (~*page_entry & PT_PRESENT) {
                if (!alloc_flag) {
                        DPRINTF("[ERROR][PAGING] not allocated page 0x%x at index "
                                "0x%x (%d), can't be accessed\n",
                                addr, pt_index, pt_index);
                        return NULL;
                }

                /*
                  pt_add_entry(page_entry, pmm_alloc(), PT_PRESENT | PT_READ_WRITE); */
                /* tmp solution */
                pt_add_entry(page_entry, pmm_alloc(), PT_PRESENT | PT_READ_WRITE | PT_USER);
        }

        return page_entry;
}

/* WARNING! different from precedent functions, it never allocates */
uintptr_t
page_get_phys_addr(pd_entry_t *page_dir, uintptr_t addr)
{
        page_entry_t *pt_entry = page_get_pt_entry(page_dir, addr, NO_ALLOC);
        
        return ((uintptr_t)*pt_entry & 0xFFFFF000) | (addr & 0xFFF);
}

/* WARNING! can only access the addresses contained in a single table
            and the table should be already be present */
static void
page_identity_pte(page_entry_t *page_table, uintptr_t addr, uintptr_t size)
{
        page_entry_t *entry = page_table + page_get_pt_index(addr);
        page_entry_t *last_entry = page_table + page_get_pt_index(addr + size);

        DPRINTF("[PAGING] identity mapping page table 0x%x from index %d "
                "(addr 0x%x) to index %d (addr 0x%x)\n",
                page_table, entry - page_table, addr, last_entry - page_table,
                addr + size);
        
        for (; entry <= last_entry; ++entry, addr += PAGE_FRAME_SIZE)
                pt_add_entry(entry, (void*)addr, PT_PRESENT | PT_READ_WRITE | PT_USER);
}
        
void
page_identity_map(pd_entry_t *page_dir, uintptr_t addr, uintptr_t size)
{
        uintptr_t end_addr = addr + size;
        DPRINTF("[PAGING] identity mapping from addr 0x%x to 0x%x\n", addr, end_addr);

        uintptr_t page_size;
        for (; addr < end_addr; addr += page_size, size -= page_size) {
                page_entry_t *page_table = page_get_pt(page_dir, addr, PAGE_ALLOC);

                /* max size that can be identity mapped in this table */
                uintptr_t max_size = PAGE_MEMORY - (addr & 0xFFF);
                /* size that should be identity mapped */
                page_size = (size > max_size) ? max_size : size; 

                page_identity_pte(page_table, addr, page_size);
        }
}

void
page_init(void)
{
        kprintf("[PAGING] setup STARTING\n");
        kprintf("[PAGING] setting up recursive paging\n");
        /* recursive paging */
        pd_add_table(page_directory + NUM_OF_ENTRIES - 1, CVIR2PHY(page_directory),
                     PD_PRESENT | PD_READ_WRITE | PD_USER);
        
        /* identity mapping all lower memory for easier access to ports,
           using static allocated page table because there isn't anyway to 
           allocate pages yet, don't confuse page_table1 with table in boot.S
           with similar name */
        pd_add_table(page_directory + 0, CVIR2PHY(page_table1), PD_PRESENT | PD_READ_WRITE | PD_USER);
        page_identity_pte((page_entry_t*)&page_table1, 0, UPPER_MEMORY_START);

        kprintf("[PAGING] setup COMPLETE\n");
}
