#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

typedef struct {
    uint64_t addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bpp;
    uint8_t *backbuf;
} framebuffer_t;

extern framebuffer_t fb;

typedef struct {
    uint32_t x, y, w, h;
} rect_t;

typedef struct {
    uint8_t r, g, b;
} color_rgb_t;

void fb_init(uint64_t mb2_info);
void fb_putpixel(int32_t x, int32_t y, uint32_t color);
void fb_putpixel_back(int32_t x, int32_t y, uint32_t color);
uint32_t fb_getpixel(int32_t x, int32_t y);
void fb_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
void fb_rect_outline(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color, int32_t thickness);
void fb_fill(uint32_t color);
void fb_draw_char(int32_t x, int32_t y, char c, uint32_t color);
void fb_draw_string(int32_t x, int32_t y, const char *s, uint32_t color);
void fb_draw_string_scaled(int32_t x, int32_t y, const char *s, uint32_t color, int32_t scale);
int32_t fb_string_width(const char *s);
int32_t fb_string_width_scaled(const char *s, int32_t scale);
void fb_hline(int32_t x, int32_t y, int32_t w, uint32_t color);
void fb_vline(int32_t x, int32_t y, int32_t h, uint32_t color);
void fb_rect_rounded(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color);
void fb_rect_gradient_v(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c_top, uint32_t c_bot);
void fb_rect_gradient_h(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c_left, uint32_t c_right);
void fb_shadow(int32_t x, int32_t y, int32_t w, int32_t h, int32_t radius, int32_t spread);
void fb_copy_back(void);
void fb_copy_region(int32_t x, int32_t y, int32_t w, int32_t h);

uint32_t fb_color(uint8_t r, uint8_t g, uint8_t b);
uint32_t fb_blend(uint32_t c1, uint32_t c2, uint8_t alpha);
uint32_t fb_darken(uint32_t color, uint8_t amount);
uint32_t fb_lighten(uint32_t color, uint8_t amount);

#endif
