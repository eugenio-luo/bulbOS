#ifndef ARCH_I386_VGA_H
#define ARCH_I386_VGA_H

#include <stdint.h>

typedef enum {
	VGA_BLACK = 0,
	VGA_BLUE = 1,
	VGA_GREEN = 2,
	VGA_CYAN = 3,
	VGA_RED = 4,
	VGA_MAGENTA = 5,
	VGA_BROWN = 6,
	VGA_LIGHT_GREY = 7,
	VGA_DARK_GREY = 8,
	VGA_LIGHT_BLUE = 9,
	VGA_LIGHT_GREEN = 10,
	VGA_LIGHT_CYAN = 11,
	VGA_LIGHT_RED = 12,
	VGA_LIGHT_MAGENTA = 13,
	VGA_LIGHT_BROWN = 14,
	VGA_WHITE = 15,
} vga_color_t;

static inline uint8_t
vga_entry_color(vga_color_t foreground, vga_color_t background)
{
        return (uint8_t)(foreground | background << 4);
}

static inline uint16_t
vga_entry(unsigned char character, uint8_t color)
{
	return (uint16_t)(character | (uint16_t)color << 8);
}

#endif
