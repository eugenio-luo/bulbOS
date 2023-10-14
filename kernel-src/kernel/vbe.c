#include <stdio.h>
#include <stdlib.h>

#include <kernel/vbe.h>
#include <kernel/multiboot.h>
#include <kernel/page.h>

extern multiboot_info_t *mb_info_ptr;

vbe_info_t *vbe_info;

void
vbe_init(void)
{
        if (~mb_info_ptr->flags & MULTIBOOT_INFO_FRAMEBUFFER) {
                kprintf("[ERROR][PMM] multiboot flags bit 12 isn't set\n");
                abort();
        }

        kprintf("addr: %x\n", mb_info_ptr->framebuffer_addr);
        kprintf("width: %d\n", mb_info_ptr->framebuffer_width);
        kprintf("height: %d\n", mb_info_ptr->framebuffer_height);
        kprintf("height: %d\n", mb_info_ptr->framebuffer_pitch);

        /*
        uint32_t *framebuffer = (uint32_t *) mb_info_ptr->framebuffer_addr;
        page_identity_map(page_directory, (uintptr_t)framebuffer, 100);
        framebuffer[0] = 0x7800;
        */
}
