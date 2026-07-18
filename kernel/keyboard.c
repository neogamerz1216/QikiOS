#include <stdint.h>
#include "io.h"
#include "framebuffer.h"

extern void printk(const char *fmt, ...);

void wm_forward_keypress(char c, uint8_t scancode);

static uint8_t shift_pressed = 0;
static uint8_t caps_lock = 0;
static uint8_t ctrl_pressed = 0;

static const char scancode_to_ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

static const char scancode_to_ascii_shift[] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0, 0,
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

void keyboard_handler() {
    uint8_t scancode = inb(0x60);

    if (scancode == 0xE0) return;

    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return;
    }
    if (scancode == 0x1D) {
        ctrl_pressed = 1;
        return;
    }
    if (scancode == 0x9D) {
        ctrl_pressed = 0;
        return;
    }

    if (scancode == 0x3A) {
        if (scancode < 0x80) caps_lock = !caps_lock;
        return;
    }

    if (scancode & 0x80) return;

    if (scancode >= sizeof(scancode_to_ascii)) return;

    char c;
    if (shift_pressed) {
        c = scancode_to_ascii_shift[scancode];
    } else {
        c = scancode_to_ascii[scancode];
    }

    if (c == 0) return;

    if (caps_lock && !shift_pressed) {
        if (c >= 'a' && c <= 'z') c = c - 32;
    } else if (caps_lock && shift_pressed) {
        if (c >= 'A' && c <= 'Z') c = c + 32;
    }

    if (scancode == 0x0E) {
        wm_forward_keypress('\b', scancode);
        return;
    }

    wm_forward_keypress(c, scancode);
    printk("%c", c);
}
