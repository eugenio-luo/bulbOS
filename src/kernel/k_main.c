#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <kernel/debug.h>
#include <kernel/pmm.h>
#include <kernel/page.h>
#include <kernel/vmm.h>
#include <kernel/pci.h>
#include <kernel/ide.h>
#include <kernel/rsdt.h>
#include <kernel/gdt.h>
#include <kernel/tss.h>
#include <kernel/idt.h>
#include <kernel/isrs.h>
#include <kernel/apic.h>
#include <kernel/pic.h>
#include <kernel/hpet.h>
#include <kernel/irq.h>
#include <kernel/cmos.h>
#include <kernel/rtc.h>
#include <kernel/ps2controller.h>
#include <kernel/keyboard.h>
#include <kernel/pit.h>
#include <kernel/task.h>
#include <kernel/mutex.h>
#include <kernel/tty.h>
#include <kernel/serialport.h>
#include <kernel/shell.h>
#include <kernel/filesystem.h>
#include <kernel/inode.h>
#include <kernel/dir.h>
#include <kernel/bio.h>
#include <kernel/file.h>
#include <kernel/syscall.h>
#include <kernel/stdio_handler.h>

/* TODO: rewrite libc, pic.c, apic.c, pci.c, ide.c */

/*
#include <kernel/vfs.h>
#include <kernel/vbe.h>
*/

int
kernel_main(void)
{
        page_init();
        pmm_init();
        vmm_init();
        multitask_init();

        terminal_initialize();
        serial_initialize();
        //vbe_init();
        
        gdt_init();
        tss_init();

        idt_init();
        isrs_init();
        syscall_init();
        
        rsdt_init();
        hpet_init();

        pic_init();
        STI();
        apic_init();

        pci_init();
        ide_init();

        bio_init();
        ext2_init(0);
        inode_init();
        dir_init();
        file_init();
        
        ps2_init();
        keyboard_init();

        stdio_init();

        printf("welcome to BulbOS\n\n");
        kprintf("boot success :)\n");

        task_info_t *task = task_user_create_new(shell_main, "shell");
        task_add_node(task);

        for (;;) 
                asm volatile ("hlt");
}

