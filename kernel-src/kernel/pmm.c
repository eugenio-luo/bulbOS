#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <kernel/pmm.h>
#include <kernel/multiboot.h>
#include <kernel/memory.h>
#include <kernel/page.h>
#include <kernel/debug.h>

extern char _kernel_start, _kernel_end;

multiboot_info_t *mb_info_ptr;
static uint8_t *mem_bitmap;
static uint32_t num_of_pages;

static uint32_t 
pmm_detect_upper_memory_size(void)
{
        if (~mb_info_ptr->flags & MULTIBOOT_INFO_MEMORY) {
                kprintf("[ERROR][PMM] multiboot flags bit 0 isn't set\n");
                abort();
        }

        kprintf("[PMM] available lower memory: 0x%x (%d) KiB\n",
                mb_info_ptr->mem_lower, mb_info_ptr->mem_lower);
        kprintf("[PMM] available upper memory: 0x%x (%d) KiB\n",
                mb_info_ptr->mem_upper, mb_info_ptr->mem_upper);

        return mb_info_ptr->mem_upper;
}

static mb_mmap_entry_t*
pmm_detect_mmap_ptr(void)
{
        if (~mb_info_ptr->flags & MULTIBOOT_INFO_MEM_MAP) {
                kprintf("[ERROR][PMM] multiboot flags bit 6 isn't set\n");
                abort();
        }

        kprintf("[PMM] memory map address: %x; ", mb_info_ptr->mmap_addr);
        kprintf("memory map length: %d\n", mb_info_ptr->mmap_length);
        kprintf("[PMM] sizeof memory map entry: %d\n", sizeof(mb_mmap_entry_t));

        return (mb_mmap_entry_t*) mb_info_ptr->mmap_addr;
}

static uint8_t
pmm_index_to_bitmap_ptr(uint8_t **ptr, uint32_t page_index)
{
        *ptr = mem_bitmap + (page_index / 8);
        return (page_index % 8);
}


static void
pmm_set_bit(uint32_t page_index)
{
        uint8_t *page;
        uint8_t bit = pmm_index_to_bitmap_ptr(&page, page_index);
        *page |= 1 << bit; 
}

/*
static void
pmm_set_bits(uint32_t page_index, uint32_t size)
{
        for (uint32_t i = 0; i < size; ++i)
                pmm_set_bit(page_index + i);
}
*/

static void
pmm_clear_bit(uint32_t page_index)
{
        uint8_t *page;
        uint8_t bit = pmm_index_to_bitmap_ptr(&page, page_index);
        *page &= ~(1 << bit);
}

/*
static void
pmm_clear_bits(uint32_t page_index, uint32_t size)
{
        for (uint32_t i = 0; i < size; ++i)
                pmm_clear_bit(page_index + i);
}
*/

/* if the page is free, it is cleared, otherwise it remains set.
   uint32_t occupied_size is the size occupied by kernel and bitmap that 
   shouldn't be cleared in the bitmap. */
static void
pmm_setup_bitmap(mb_mmap_entry_t *mmap_entry, uint32_t occupied_size)
{
        uint32_t num_of_entries = mb_info_ptr->mmap_length / sizeof(mb_mmap_entry_t);
        mb_mmap_entry_t *last_entry = mmap_entry + num_of_entries;

        kprintf("[PMM] MEMORY MAP:\n");
        for (; mmap_entry < last_entry; ++mmap_entry) {
                uint64_t addr = mmap_entry->addr_low + (1ULL * mmap_entry->addr_high << 32);
                uint64_t size = mmap_entry->len_low + (1ULL * mmap_entry->len_high << 32);

                kprintf("[PMM] base address: 0x%x; ", addr);
                kprintf("size: 0x%x; ", size);
                kprintf("type: %s\n", (char*[]){"AVAILABLE", "RESERVED"}[mmap_entry->type - 1]);

                /* lower memory isn't cleared, can't never be allocated */
                if (addr < UPPER_MEMORY_START) 
                        continue;

                if (mmap_entry->type != MULTIBOOT_MEM_AVAILABLE)
                        continue;

                /* make it so that kernel + bitmap aren't cleared in the bitmap,
                   so they can't be allocated and corrupted */
                if (addr == UPPER_MEMORY_START) {
                        addr += occupied_size;
                        size -= occupied_size;
                }

                uint32_t bit = addr / PAGE_FRAME_SIZE;
                uint32_t end = (addr + size) / PAGE_FRAME_SIZE + 1;
                
                for (; bit <= end; ++bit)
                        pmm_clear_bit(bit);
        }
}

/*
static int
pmm_is_clear(uint32_t page_index)
{
        uint8_t *page;
        uint8_t bit = pmm_index_to_bitmap_ptr(&page, page_index);
        return ~(*page >> bit) & 1;
}
*/

static int
pmm_is_set(uint32_t page_index)
{
        uint8_t *page;
        uint8_t bit = pmm_index_to_bitmap_ptr(&page, page_index);
        return (*page >> bit) & 1;
}

/* this function exists because the allocator has to loop back when 
   hitting the end instead to going further, this helper functions makes
   my life easier */
static uint32_t
pmm_add(uint32_t *last_allocated, uint32_t value)
{
        *last_allocated = (*last_allocated + value) % num_of_pages;
        return *last_allocated;
}

static inline uint32_t*
pmm_get_dword(uint32_t page_index)
{
        return (uint32_t*)mem_bitmap + (page_index / 32);
}

static uint32_t
pmm_search_free_page(uint32_t last_alloc)
{
        /* searching for a dword that has a free page */
        uint32_t start = last_alloc;
        uint32_t *page = pmm_get_dword(last_alloc);

        for (; *page == BIT32_MAX; page = pmm_get_dword(last_alloc)) {
                pmm_add(&last_alloc, 32);
                /* to avoid an endless loop, when it loop back to the start
                   it can be sure that there aren't any free page */
                if (last_alloc != start) continue;
                
                kprintf("[ERROR][PMM] Not enough physical memory\n");
                return BIT32_MAX;
        }

        /* searching for the free page */
        for (; pmm_is_set(last_alloc); pmm_add(&last_alloc, 1));

        return last_alloc;
}

void*
pmm_alloc(void)
{
        static uint32_t last_alloc = 0;

        last_alloc = pmm_search_free_page(last_alloc);
        if (last_alloc == BIT32_MAX) return NULL;
        
        pmm_set_bit(last_alloc);
        DPRINTF("[PMM] allocating free page n. 0x%x, address: %x\n",
                last_alloc, last_alloc * PAGE_FRAME_SIZE);
        return (void*)(last_alloc * PAGE_FRAME_SIZE);
}

void
pmm_free(void *page)
{
        uint32_t index = (uintptr_t)page / PAGE_FRAME_SIZE;
        DPRINTF("[PMM] freeing occupied page n. 0x%x, address: %x\n",
                index, page);
        pmm_clear_bit(index);
}

/* pmm_allocs seems useless for now, because it never has been used for anything */
/*
void*
pmm_allocs(size_t size)
{
        static uint32_t last_alloc = 0;
        if (!size) return NULL;

        last_alloc = pmm_search_free_page(last_alloc);
}
*/

/*
void
pmm_frees(void *page, size_t size)
{
        uint32_t index = (uintptr_t)page_frame / PAGE_FRAME_SIZE;
        pmm_clear_bits(index, size);
}
*/

void
pmm_init(void)
{
        kprintf("[PMM] setup STARTING\n");

        /* UPPER_MEMORY_START get shifted right by 10 because we want result in KiB */
        uint32_t memory_size = pmm_detect_upper_memory_size() + (UPPER_MEMORY_START >> 10);
        kprintf("[PMM] total memory size: 0x%x KiB\n", memory_size);

        /* memory_size >> 2 == memory_size * 1024 (1024 bytes = KiB) / 4096 (page size) */
        num_of_pages = memory_size >> 2;
        kprintf("[PMM] total num of pages: 0x%x (%d)\n", num_of_pages, num_of_pages);

        /* num_of_pages / 8 because a byte can contain info about 8 pages */
        uint32_t bitmap_size = num_of_pages >> 3;
        kprintf("[PMM] BITMAP size: 0x%x bytes\n", bitmap_size);

        uintptr_t kernel_start = (uintptr_t)&_kernel_start;
        uintptr_t kernel_end = (uintptr_t)&_kernel_end;
 
        uint32_t kernel_size = kernel_end - PHY2VIR(kernel_start);
        uint32_t total_size = bitmap_size + kernel_size + kernel_start;
        kprintf("[PMM] KERNEL size: 0x%x bytes\n", kernel_size);
        kprintf("[PMM] TOTAL occupied size: 0x%x bytes\n", total_size);

        mem_bitmap = (uint8_t*)kernel_end + 1;
        /* setting all the bitmap occupied */
        memset(mem_bitmap, 0xFF, bitmap_size);

        /* clear the bitmap according to the memory map */
        mb_mmap_entry_t *mmap_head = pmm_detect_mmap_ptr();
        pmm_setup_bitmap(mmap_head, total_size);
        
        kprintf("[PMM] setup COMPLETE\n");
}
