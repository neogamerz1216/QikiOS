#include <stdint.h>
#include "io.h"

static uint16_t *vga_buffer = (uint16_t*)0xB8000;
static int cursor_x = 0;
static int cursor_y = 0;
static const int VGA_WIDTH = 80;
static const int VGA_HEIGHT = 25;

static void update_cursor() {
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_init() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = (0x0F << 8) | ' ';
    }
    cursor_x = 0;
    cursor_y = 0;
    update_cursor();
}

void vga_putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~7;
    } else {
        vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = (0x0F << 8) | c;
        cursor_x++;
    }
    
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    
    if (cursor_y >= VGA_HEIGHT) {
        for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
            vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
        }
        for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            vga_buffer[i] = (0x0F << 8) | ' ';
        }
        cursor_y = VGA_HEIGHT - 1;
    }
    
    update_cursor();
}

void vga_puts(const char *s) {
    while (*s) vga_putchar(*s++);
}
