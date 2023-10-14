/* void task_switch_kernel(task_info_t *new) */

#define AVAILABLE          $0
#define RUNNING            $1
#define BLOCKED            $2
#define SLEEPING           $3
#define TERMINATED         $4
#define WAITING_FOR_LOCK   $5        

/* task_info_t */
#define PID         0x0
#define ESP         0x4
#define ESP0        0x8
#define EBP         0xC
#define PAGE_DIR    0x10
#define STATE       0x14
#define NEXT        0x18
#define CURR_DIR    0x1C
#define TIME_USED   0x20
#define WAKE_UP     0x28

/* tss_t */
#define TSS_ESP0    0x4
        
        .global task_switch
        .extern current_task
        .extern tss
task_switch:
        cli
        pushl  %ebp
        pushl  %ebx
        pushl  %esi
        pushl  %edi
        
        movl   (current_task), %edi
        movl   %esp, ESP(%edi)

        /* 
        *  why 0x14? because 4 regs got pushed on the stack, so function argument
         *  is (4 + 1) * 4 away from ESP
         */
        
        movl   0x14(%esp), %esi
        movl   %esi, (current_task)

        movl   $0, NEXT(%esi) 
        movl   RUNNING, STATE(%esi) 
        movl   ESP(%esi), %esp
        movl   PAGE_DIR(%esi), %eax
        movl   ESP0(%esi), %ebx

        movl   tss, %edx
        movl   %ebx, TSS_ESP0(%edx)
        
        movl   %cr3, %ecx
        cmpl   %ecx, %eax
        je     .equalPageDir
        movl   %eax, %cr3

.equalPageDir:        
        popl   %edi
        popl   %esi
        popl   %ebx
        popl   %ebp

        /* set flag for interrupt everytime it switch, don't know but sometimes
           flag get cleared */
        sti 
        ret
