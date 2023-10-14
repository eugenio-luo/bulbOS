#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "vga.h"
#include <kernel/tty.h>
#include <kernel/page.h>

#define VGA_WIDTH    80
#define VGA_HEIGHT   25
#define VGA_MEMORY   (uint16_t*) 0xB8000

static size_t terminal_row = 0;
static size_t terminal_column = 0;
static uint8_t terminal_color = 0;
static uint16_t* terminal_buffer = VGA_MEMORY;

static void
terminal_zeroline(size_t y)
{
        uint16_t *entry = terminal_buffer + y * VGA_WIDTH;
        uint16_t *last_entry = entry + VGA_WIDTH;

        for (; entry < last_entry; ++entry)
                *entry = vga_entry(' ', terminal_color);
}

void
terminal_initialize(void)
{
        kprintf("[TERMINAL] setup STARTING\n");

        terminal_color = vga_entry_color(VGA_WHITE, VGA_BLACK);
        
        for (size_t y = 0; y < VGA_HEIGHT; y++)
                terminal_zeroline(y);

        kprintf("[TERMINAL] setup COMPLETE\n");
}

void
terminal_setcolor(uint8_t color)
{
	terminal_color = color;
}

void
terminal_entry(unsigned char c, uint8_t color, size_t x, size_t y)
{
	uint16_t *entry = terminal_buffer + y * VGA_WIDTH + x;
	*entry = vga_entry(c, color);
}

static void
terminal_copyline(size_t dst_y, size_t src_y)
{
        for (size_t x = 0; x < VGA_WIDTH; ++x) {
                const unsigned char c = terminal_buffer[src_y * VGA_WIDTH + x];
                terminal_entry(c, terminal_color, x, dst_y);
        }
}

static void
terminal_scroll()
{
        for (size_t y = 0; y < VGA_HEIGHT - 1; ++y)
                terminal_copyline(y, y + 1);

        terminal_zeroline(VGA_HEIGHT - 1);
        --terminal_row;
}

static void
terminal_newline()
{
        terminal_column = 0;
        
        if (++terminal_row == VGA_HEIGHT)
                terminal_scroll();
}

static void
terminal_back()
{
        if (!terminal_column) {
                terminal_entry(' ', terminal_color, terminal_column, terminal_row--);
                terminal_column = VGA_WIDTH - 1;
                return;
        }
        
        terminal_entry(' ', terminal_color, --terminal_column, terminal_row);
}        

void
terminal_update_cursor(uint8_t x, uint8_t y)
{
        uint16_t pos = y * VGA_WIDTH + x;

        outb(0xF, 0x3D4);
        outb((uint8_t)(pos & 0xFF), 0x3D5);
        outb(0xE, 0x3D4);
        outb((uint8_t)(pos >> 8), 0x3D5);
}

void
terminal_putchar(char c)
{
	unsigned char uc = c;

        switch (c) {
        case '\n':
                terminal_newline();
                break;

        case '\b':
                terminal_back();
                break;

        default:
                terminal_entry(uc, terminal_color, terminal_column, terminal_row);

                if (++terminal_column == VGA_WIDTH)
                        terminal_newline();
        }

        terminal_update_cursor(terminal_column, terminal_row);
}

void
terminal_write(const char* data, size_t size)
{
        const char *entry = data;
        const char *last_entry = entry + size;

        for (; entry < last_entry; ++entry)
                terminal_putchar(*entry);
}

void
terminal_writestring(const char* data)
{
	terminal_write(data, strlen(data));
}

/*
uint16_t
terminal_get_cursor(void)
{
        // TODO: implementation
} 
*/       

