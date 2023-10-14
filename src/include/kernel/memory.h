#ifndef _KERNEL_MEMORY_H
#define _KERNEL_MEMORY_H

#define KERNEL_OFFSET        0xC0000000
#define PAGE_FRAME_SIZE      4096

#define EBDA_START           0x40E
#define MAIN_BIOS_START      0xE0000
#define MAIN_BIOS_END        0xFFFFF
#define UPPER_MEMORY_START   0x100000
#define KHEAP_LIST_START     0xD0000000
#define KHEAP_BLOCK_START    (0xE0000000 - 0x1000)
#define PDE_LAST_INDEX_START 0xFFC00000
#define PAGE_LAST_DWORD      ((PAGE_FRAME_SIZE - 1) / 4)

#define VIR2PHY(addr)        (addr - KERNEL_OFFSET)
#define PHY2VIR(addr)        (addr + KERNEL_OFFSET)
#define CVIR2PHY(addr)       (void*) VIR2PHY((uintptr_t)addr)
#define CPHY2VIR(addr)       (void*) PHY2VIR((uintptr_t)addr)
#define KIB(x)               (1024 * x)

#define ALIGN_ADDR(addr, align)   ((addr + (align - 1)) & ~(align - 1))

#endif
