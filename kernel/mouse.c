#include <stdint.h>
#include "io.h"
#include "framebuffer.h"
#include "window_manager.h"

extern void printk(const char *fmt, ...);

void wm_handle_click(int32_t mx, int32_t my, uint8_t button);
void wm_handle_drag(int32_t mx, int32_t my);
void wm_handle_drag_end(void);
void wm_handle_resize(int32_t mx, int32_t my);
void wm_handle_resize_end(void);

#define CURSOR_W 12
#define CURSOR_H 16
#define CURSOR_SCALE 2
#define CURSOR_SCALED_W (CURSOR_W * CURSOR_SCALE)
#define CURSOR_SCALED_H (CURSOR_H * CURSOR_SCALE)

static int8_t mouse_byte[3];
static uint8_t mouse_cycle = 0;
static int32_t mouse_x = 512;
static int32_t mouse_y = 384;
static uint8_t mouse_left_state = 0;
static uint8_t mouse_right_state = 0;

static const uint16_t cursor_sprite[CURSOR_H] = {
    0b1000000000000000,
    0b1100000000000000,
    0b1110000000000000,
    0b1111000000000000,
    0b1111100000000000,
    0b1111110000000000,
    0b1111111000000000,
    0b1111111100000000,
    0b1111111110000000,
    0b1111111000000000,
    0b1110110000000000,
    0b1100110000000000,
    0b1000011000000000,
    0b0000011000000000,
    0b0000001100000000,
    0b0000001100000000,
};

static const uint16_t cursor_mask[CURSOR_H] = {
    0b1100000000000000,
    0b1110000000000000,
    0b1111000000000000,
    0b1111100000000000,
    0b1111110000000000,
    0b1111111000000000,
    0b1111111100000000,
    0b1111111110000000,
    0b1111111111000000,
    0b1111111111000000,
    0b1111111110000000,
    0b1111011110000000,
    0b1110011100000000,
    0b1100011100000000,
    0b1000001100000000,
    0b0000001100000000,
};

void mouse_wait(uint8_t type) {
    int timeout = 100000;
    if (type == 0) {
        while (timeout--) {
            if (inb(0x64) & 1) return;
        }
    } else {
        while (timeout--) {
            if (!(inb(0x64) & 2)) return;
        }
    }
}

void mouse_write(uint8_t val) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, val);
}

uint8_t mouse_read() {
    mouse_wait(0);
    return inb(0x60);
}

void mouse_init() {
    mouse_wait(1);
    outb(0x64, 0xA8);

    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    uint8_t status = inb(0x60) | 2;
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);

    mouse_write(0xF6);
    mouse_read();

    mouse_write(0xF4);
    mouse_read();

    printk("Mouse initialized\n");
}

int32_t mouse_get_x(void) {
    return mouse_x;
}

int32_t mouse_get_y(void) {
    return mouse_y;
}

void mouse_draw_cursor(void) {
    for (int32_t y = 0; y < CURSOR_H; y++) {
        for (int32_t x = 0; x < CURSOR_W; x++) {
            uint8_t is_sprite = (cursor_sprite[y] >> (15 - x)) & 1;
            uint8_t is_mask = (cursor_mask[y] >> (15 - x)) & 1;

            for (int32_t sy = 0; sy < CURSOR_SCALE; sy++) {
                for (int32_t sx = 0; sx < CURSOR_SCALE; sx++) {
                    int32_t px = mouse_x + x * CURSOR_SCALE + sx;
                    int32_t py = mouse_y + y * CURSOR_SCALE + sy;

                    if (is_sprite) {
                        fb_putpixel(px, py, 0xFFFFFF);
                    } else if (is_mask) {
                        fb_putpixel(px, py, 0x000000);
                    }
                }
            }
        }
    }

    for (int32_t y = 0; y < CURSOR_SCALED_H; y++) {
        fb_putpixel(mouse_x + CURSOR_SCALED_W, mouse_y + y, fb_color(0x20, 0x20, 0x20));
    }
    for (int32_t x = 0; x <= CURSOR_SCALED_W; x++) {
        fb_putpixel(mouse_x + x, mouse_y + CURSOR_SCALED_H, fb_color(0x20, 0x20, 0x20));
    }
}

void mouse_handler() {
    uint8_t data = inb(0x60);

    switch (mouse_cycle) {
        case 0:
            if ((data & 0x08) == 0) return;
            mouse_byte[0] = data;
            mouse_cycle++;
            break;
        case 1:
            mouse_byte[1] = data;
            mouse_cycle++;
            break;
        case 2:
            mouse_byte[2] = data;

            int32_t dx = (int32_t)mouse_byte[1] * 3;
            int32_t dy = -(int32_t)mouse_byte[2] * 3;

            mouse_x += dx;
            mouse_y += dy;

            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= (int32_t)fb.width - CURSOR_SCALED_W) mouse_x = fb.width - CURSOR_SCALED_W;
            if (mouse_y >= (int32_t)fb.height - CURSOR_SCALED_H) mouse_y = fb.height - CURSOR_SCALED_H;

            uint8_t left = mouse_byte[0] & 0x01;
            uint8_t right = (mouse_byte[0] >> 1) & 0x01;

            if (left && !mouse_left_state) {
                wm_handle_click(mouse_x, mouse_y, 0);
            }
            if (right && !mouse_right_state) {
                wm_handle_click(mouse_x, mouse_y, 1);
            }

            if (left && mouse_left_state) {
                wm_handle_drag(mouse_x, mouse_y);
                wm_handle_resize(mouse_x, mouse_y);
            }

            if (!left && mouse_left_state) {
                wm_handle_drag_end();
                wm_handle_resize_end();
            }

            mouse_left_state = left;
            mouse_right_state = right;

            wm.needs_redraw = 1;

            mouse_cycle = 0;
            break;
    }
}
