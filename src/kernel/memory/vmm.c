#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

#include <kernel/vmm.h>
#include <kernel/page.h>
#include <kernel/memory.h>
#include <kernel/debug.h>

static node_t *kheap_head;
static uintptr_t kheap_start;
static uintptr_t kheap_end;

static node_t *kheap_stack_last;
static uintptr_t kheap_stack_start;
static uintptr_t kheap_stack_end;

static void
vmm_kheap_list_init(uintptr_t heap_start)
{
        kprintf("[VMM] kernel heap placed at address 0x%x\n", heap_start);
        
        page_get_pt_entry(page_directory, heap_start, PAGE_ALLOC);
        kheap_start = heap_start;
        kheap_end = heap_start + PAGE_FRAME_SIZE;
        
        kheap_head = (node_t*)heap_start;
        kheap_head->size = PAGE_FRAME_SIZE - sizeof(node_t);
        kheap_head->next = NULL;
}

static inline void*
vmm_node_ret_addr(node_t *kheap_node)
{
        return (char*)kheap_node + sizeof(node_t) + kheap_node->size;
}

/* sorted insert to make merging easier later */
static void
vmm_sorted_insert(node_t *head, node_t *inserted)
{
        for (; head->next; head = head->next)
                if (head->next > inserted)
                        break;

        node_t *tmp = head->next;
        head->next = inserted;
        inserted->next = tmp;
}

/* nodes have to be sorted to use this function, merging nodes
   is useful because otherwise there would be extreme fragmentation
   between the nodes after a while and there would never be a 
   node bigger than PAGE_FRAME_SIZE */
static void
vmm_merge_free_block(node_t *node)
{
        while (node->next) {
                uintptr_t node_end = (uintptr_t)node + node->size + sizeof(node_t); 

                if (node_end == (uintptr_t)node->next) {
                        node->size += node->next->size + sizeof(node_t);
                        node->next = node->next->next;
                } else {
                        node = node->next;
                }
        }
}

/* search for a node that has enough space */
static node_t*
vmm_get_node(node_t *node, node_t **prev, size_t size)
{
        for (; node; *prev = node, node = node->next)
                if (node->size > size)
                        return node;

        return NULL;
}

/* every node has a size variable and pointer to next one, the allocator
   goes each one trying to find the FIRST one that has enough space */
static void*
vmm_list_alloc(size_t size)
{
        node_t *prev = NULL;
        size += sizeof(size_t);
        node_t *node = vmm_get_node(kheap_head, &prev, size);

        /* if the kheap can't accommodate the request, the kheap is enlarged */
        while (!node) {

                if (kheap_end > kheap_stack_end) {
                        kprintf("kheap overflowed into kheap stack\n");
                        return NULL;
                }
                page_get_pt_entry(page_directory, kheap_end, PAGE_ALLOC);

                node_t *new_node = (node_t*)kheap_end;
                new_node->size = PAGE_FRAME_SIZE - sizeof(node_t);
                new_node->next = NULL;

                /* no sorted insert here because it is assured that new
                   node is the last node */
                node_t *tmp = kheap_head;
                for (; tmp->next; tmp = tmp->next);
                tmp->next = new_node;
                
                kheap_end += PAGE_FRAME_SIZE;
                vmm_merge_free_block(kheap_head);

                node = vmm_get_node(kheap_head, &prev, size);
        }

        node->size -= size;
        /* get ptr to size ptr, every allocated ptr has at -0x4 offset
           the size of the pointer, it is used in kfree, otherwise it doesn't
           know how much to free */
        size_t *size_ptr = (size_t*)vmm_node_ret_addr(node);

        /* replacing node with pointer header ON PURPOSE */
        if (prev && !node->size) {
                size_ptr = (size_t*)((char*)size_ptr - sizeof(node_t)); 
                prev->next = node->next;
                size += sizeof(node_t);
        }

        *size_ptr = size;

        DPRINTF("[VMM] allocated 0x%x bytes of memory at address 0x%x\n",
                size, size_ptr + 1);

        return (void*)(size_ptr + 1);
}

static void
vmm_kheap_stack_init(uintptr_t heap_stack_start)
{
        kprintf("[VMM] kernel block heap placed at address 0x%x\n", heap_stack_start);
        kheap_stack_start = heap_stack_start + PAGE_FRAME_SIZE;
        kheap_stack_end = heap_stack_start;
        page_get_pt_entry(page_directory, kheap_stack_end, PAGE_ALLOC);
        
        kheap_stack_last = kmalloc(sizeof(node_t));
        kheap_stack_last->addr = kheap_stack_end;
        kheap_stack_last->next = NULL;
}

/* it only allocates page sized memory block, it behaves like a stack 
   with nodes containing info about free pages */
static void*
vmm_block_alloc(void)
{
        if (kheap_stack_end < kheap_end) {
                kprintf("[ERROR][VMM] page stack overflowed in kheap\n");
                return NULL;
        }
        
        /* if there are free page on stack, allocate the page, if not allocate
           a new one by enlarging the kheap */
        if (kheap_stack_last) {
                node_t *tmp = kheap_stack_last;
                kheap_stack_last = kheap_stack_last->next;
                
                uintptr_t addr = tmp->addr;
                kfree(tmp);

                DPRINTF("[VMM] allocated 0x1000 bytes of memory at address 0x%x\n",
                        addr);
                return (void*)addr;
        }

        kheap_stack_end -= PAGE_FRAME_SIZE;
        page_get_pt_entry(page_directory, kheap_stack_end, PAGE_ALLOC);

        DPRINTF("[VMM] allocated 0x1000 bytes of memory at address 0x%x\n",
                kheap_stack_end);

        return (void*)kheap_stack_end;
}

/* there are two allocator, one exclusive for page sized memory, while other one
   is for general usage. */
void*
kmalloc(size_t size)
{
        if (!size) return NULL;
       
        if (size == PAGE_FRAME_SIZE)
                return vmm_block_alloc();

        return vmm_list_alloc(size);
}

void
kfree(void *ptr)
{
        uintptr_t ptr_addr = (uintptr_t)ptr;

        if (ptr_addr < kheap_start || ptr_addr >= kheap_stack_start)
                return;

        if (ptr_addr > kheap_end) {
                /* it's sure that memory is from vmm block allocator */

                uintptr_t page = ptr_addr & ~0xFFF;
                node_t *node = kmalloc(sizeof(sizeof(node_t)));

                /* REMEMBER: if there is a bug with kfree it's because
                   of the designated initializer */
                node = &(node_t) {
                        .addr = page,
                        .next = kheap_stack_last,
                };

                /* freed page get pushed on the stack */
                kheap_stack_last = node;
        } else {
                /* it's sure that memory is from vmm list allocator */
                
                /* makes a new node out of the pointer */
                node_t *node = (node_t*)((char*)ptr - sizeof(size_t));
                node->size -= sizeof(node_t);
                
                /* we insert in a sorted list because it easier to merge free blocks */
                vmm_sorted_insert(kheap_head, node);
                vmm_merge_free_block(kheap_head);        
        }

        ptr = NULL;
}

void
vmm_print_kheap(void)
{
        for (node_t *node = kheap_head; node; node = node->next)
                printf("addr: %x, size: %x\n", node, node->size);
}
        
void
vmm_init(void)
{
        kprintf("[VMM] setup STARTING\n");
        
        vmm_kheap_list_init(KHEAP_LIST_START);
        vmm_kheap_stack_init(KHEAP_BLOCK_START);
        
        kprintf("[VMM] setup COMPLETE\n");
}
