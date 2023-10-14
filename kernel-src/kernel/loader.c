#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <kernel/loader.h>
#include <kernel/vmm.h>
#include <kernel/page.h>
#include <kernel/memory.h>
#include <kernel/file.h>

uintptr_t
load_new_page_dir(void)
{
        pd_entry_t *new_dir = kmalloc(PAGE_FRAME_SIZE);

        memcpy(new_dir, page_directory, PAGE_FRAME_SIZE);
        
        return (uintptr_t) new_dir;
}

/*
uintptr_t
load_new_page_dir(file_t *exec)
{
        pd_entry_t *new_dir = kmalloc(PAGE_FRAME_SIZE);

        size_t size = (size_t)-1 - KERNEL_OFFSET;
        load_page_cpy(new_dir, page_directory, KERNEL_OFFSET, size);
        
        return (uintptr_t) new_dir;
}
*/
