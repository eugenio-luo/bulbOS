ENTRY(_start)

KERNEL_START = 0x1M;
KERNEL_OFFSET = 0xC0000000;

SECTIONS
{
	. = KERNEL_START;
        _kernel_start = .;

        .multiboot.data :
        {
                *(.multiboot.data)
        }

        _text_start = .;
        
        .multiboot.text :
        {
                *(.multiboot.text)
        }

        . += KERNEL_OFFSET;

	.text ALIGN(4K) : AT ( ADDR(.text) - KERNEL_OFFSET )
	{
		*(.text)
	}

	.rodata ALIGN(4K) : AT ( ADDR(.rodata) - KERNEL_OFFSET )
	{
		*(.rodata)
	}

        _rodata_end = .;

	.data ALIGN(4K) : AT ( ADDR(.data) - KERNEL_OFFSET )
	{
		*(.data)
	}

	.bss ALIGN(4K) : AT ( ADDR(.bss) - KERNEL_OFFSET )
	{
                *(COMMON)
		*(.bss)
                *(.stack)
                *(.prdt)
        }

        _kernel_end = .;
}
