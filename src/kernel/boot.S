#include "kernel/memory.h"

/* declare constants for the multiboot header */
.set ALIGN,    1<<0             /* align loaded modules on page boundaries */
.set MEMINFO,  1<<1             /* memory info */
.set GRAPHICS, 1<<2
.set FLAGS,    ALIGN | MEMINFO 
.set MAGIC,    0x1BADB002       
.set CHECKSUM, -(MAGIC + FLAGS) 
.set WIDTH,    1024
.set HEIGHT,   768
.set DEPTH,    32        
        
/* multiboot header */
.section .multiboot.data, "aw"
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM
.skip 32 * 5
.long 1
.long WIDTH
.long HEIGHT
.long DEPTH    
        
/* kernel stack */
.section .stack, "aw", @nobits
.align 16
stack_bottom:
.skip KIB(16)
stack_top:

/* kernel page directory and initial page tables */
.section .bss, "aw", @nobits
.align KIB(4)
boot_page_dir:
.skip KIB(4)
boot_page_table1: /* used to map kernel at 0xC0000000 and initially at 0x0 */
.skip KIB(4)
        
/* TEMPORARY table for drive data transfer 
   TODO: memory allocator for aligned memory */        
.section .prdt, "aw", @nobits
.align KIB(1) 
prdt_start:
.skip KIB(1)
mem_buffer_start:
.skip KIB(4) 
        
.section .multiboot.text, "a"
.global _start
.type _start, @function
_start:
        .extern mb_info_ptr /* pmm.c, memory info */
        movl %ebx, (VIR2PHY(mb_info_ptr)) 

        .extern prd_table /* ide.c */
        .extern ide_mem_buffer
        movl $(prdt_start), (VIR2PHY(prd_table))
        movl $(mem_buffer_start), (VIR2PHY(ide_mem_buffer))
        
        .extern page_directory /* page.c */
        movl $(boot_page_dir), (VIR2PHY(page_directory))

        /* identity mapping the PMM's bitmap and kernel */
        
        /* calculate the size of PMM's bitmap, 1 bit = 1 page */
        addl $8, %ebx /* upper memory size in KILOBYTES, multiboot info offset 0x8 */
        movl (%ebx), %eax
        addl $(UPPER_MEMORY_START >> 10), %eax
        shr  $5, %eax
        /* eax get shifted left by 10 because it's the upper memory size in KIB, then
           lower memory (including reserved memory) is added to the amount.
           eax is shifted right this time by 15, because a single bit contains a page worth
           of memory (4096 = 10 ^ 12), therefore a byte contains 4096 * 8 bytes (10 ^ 15) */

        /* kernel size is added to the total */
        addl $(VIR2PHY(_kernel_end)), %eax
        
        movl $(VIR2PHY(boot_page_table1)), %edi
        movl $0, %esi
        movl $1023, %ecx
        
1:
        /* only kernel + bitmap is mapped for now, later ports, etc... will be mapped */
        cmpl $_kernel_start, %esi
        jl   4f
        cmpl %eax, %esi
        jge  5f

        /* text and rodata sections are read only */
        movl %esi, %edx
        cmpl $_text_start, %edx
        jl   2f
        cmpl $VIR2PHY(_rodata_end), %edx
        jle  3f
        
2:      
        orl  $0x2, %edx
3:
        /* tmp solution */
        orl  $0x5, %edx
              
        /* orl  $0x1, %edx */ 
        movl %edx, (%edi)

4:
        addl $4096, %esi
        addl $4, %edi
        loop 1b

5:
        /* identity map at virtual address 0x0 and 0xC0000000 */
        /* tmp solution for ring 3 */
        movl $(VIR2PHY(boot_page_table1) + 0x007), VIR2PHY(boot_page_dir) + 0 
        movl $(VIR2PHY(boot_page_table1) + 0x007), VIR2PHY(boot_page_dir) + 768 * 4 

        /*
        movl $(VIR2PHY(boot_page_table1) + 0x003), VIR2PHY(boot_page_dir) + 0
        movl $(VIR2PHY(boot_page_table1) + 0x003), VIR2PHY(boot_page_dir) + 768 * 4
        */
        
        /* set cr3 and activate paging */
        movl $(VIR2PHY(boot_page_dir)), %ecx
        movl %ecx, %cr3
        movl %cr0, %ecx
        orl  $0x80010000, %ecx
        movl %ecx, %cr0

        /* absolute jump necessary */
        lea  4f, %ecx
        jmp  *%ecx
        
.section .text
4:
        /* unmap unnecessary table at 0x0 */
        movl $0, VIR2PHY(boot_page_dir) + 0

        /* reload */
        movl %cr3, %ecx
        movl %ecx, %cr3
        
        movl $stack_top, %esp

	    call kernel_main

        /* kernel SHOULDN'T never reach here */
	    cli
1:	    hlt
	    jmp 1b
