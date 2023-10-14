#ifndef _KERNEL_PMM_H
#define _KERNEL_PMM_H

#include <stdint.h>

#define BIT32_MAX         (uint32_t)~0

void pmm_init(void);
void *pmm_alloc(void);
void *pmm_allocs(uint32_t size);
void pmm_free(void *page_frame);
void pmm_frees(void *page_frame, uint32_t size);

#endif
