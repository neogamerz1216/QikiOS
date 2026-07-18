#include "window_manager.h"
#include <stdint.h>
#include <stddef.h>

extern uint64_t timer_ticks;
extern void *page_alloc(void);

wm_state_t wm = {0};

static uint8_t start_menu_open = 0;
static uint8_t *desktop_cache = NULL;
static uint32_t desktop_cache_size = 0;

static void draw_window_close_x(int32_t cx, int32_t cy, uint32_t color) {
    fb_hline(cx - 3, cy - 3, 7, color);
    fb_hline(cx - 3, cy + 3, 7, color);
    fb_hline(cx - 2, cy - 2, 5, color);
    fb_hline(cx - 2, cy + 2, 5, color);
    fb_hline(cx - 1, cy - 1, 3, color);
    fb_hline(cx - 1, cy + 1, 3, color);
    fb_putpixel(cx, cy, color);
}

static void draw_window_minimize_line(int32_t cx, int32_t cy, uint32_t color) {
    fb_hline(cx - 3, cy + 2, 7, color);
}

static void draw_window_maximize_square(int32_t cx, int32_t cy, uint32_t color) {
    fb_rect_outline(cx - 3, cy - 2, 7, 6, color, 1);
}

void wm_init(void) {
    wm.window_count = 0;
    wm.focused_window = -1;
    wm.drag_window = -1;
    wm.dragging = 0;
    wm.resize_window = -1;
    wm.resize_edge = RESIZE_NONE;
    wm.icon_count = 0;
    wm.taskbar_count = 0;
    wm.start_item_count = 0;
    wm.ctx_item_count = 0;
    wm.ctx_menu_open = 0;
    wm.next_z_order = 0;
    wm.needs_redraw = 1;
    wm.desktop_dirty = 1;
    wm.snap_preview = 0;
    start_menu_open = 0;
}

void wm_draw_desktop(void) {
    for (int32_t y = 0; y < (int32_t)fb.height - WM_TASKBAR_HEIGHT; y++) {
        uint8_t t = (uint8_t)((int32_t)y * 255 / (int32_t)(fb.height - WM_TASKBAR_HEIGHT));
        uint32_t c = fb_blend(0x060A14, 0x020408, t);
        fb_hline(0, y, fb.width, c);
    }

    int32_t center_x = fb.width * 2 / 3;
    int32_t center_y = (fb.height - WM_TASKBAR_HEIGHT) / 2;

    for (int32_t r = 320; r > 0; r -= 3) {
        uint8_t alpha = (uint8_t)(6 * (320 - r) / 320);
        for (int32_t dy = -r; dy < r; dy += 3) {
            for (int32_t dx = -r; dx < r; dx += 3) {
                int32_t dist_sq = dx * dx + dy * dy;
                if (dist_sq >= r * r || dist_sq < (r - 8) * (r - 8)) continue;
                int32_t px = center_x + dx;
                int32_t py = center_y + dy;
                if (px >= 0 && py >= 0 && px < (int32_t)fb.width && py < (int32_t)(fb.height - WM_TASKBAR_HEIGHT)) {
                    uint32_t bg = fb_getpixel(px, py);
                    uint32_t ring = fb_color(0x10, 0x25, 0x45);
                    fb_putpixel(px, py, fb_blend(bg, ring, alpha));
                }
            }
        }
    }

    for (int32_t r = 220; r > 0; r -= 3) {
        uint8_t alpha = (uint8_t)(5 * (220 - r) / 220);
        for (int32_t dy = -r; dy < r; dy += 3) {
            for (int32_t dx = -r; dx < r; dx += 3) {
                int32_t dist_sq = dx * dx + dy * dy;
                if (dist_sq >= r * r || dist_sq < (r - 6) * (r - 6)) continue;
                int32_t px = center_x + dx;
                int32_t py = center_y + dy;
                if (px >= 0 && py >= 0 && px < (int32_t)fb.width && py < (int32_t)(fb.height - WM_TASKBAR_HEIGHT)) {
                    uint32_t bg = fb_getpixel(px, py);
                    uint32_t ring = fb_color(0x0C, 0x1A, 0x35);
                    fb_putpixel(px, py, fb_blend(bg, ring, alpha));
                }
            }
        }
    }

    for (int32_t y = 0; y < (int32_t)(fb.height - WM_TASKBAR_HEIGHT); y += 32) {
        for (int32_t x = 0; x < (int32_t)fb.width; x += 32) {
            uint32_t bg = fb_getpixel(x, y);
            fb_putpixel(x, y, fb_lighten(bg, 3));
        }
    }

    int32_t star_positions[][2] = {
        {120, 80}, {300, 160}, {700, 60}, {900, 200}, {500, 120},
        {180, 300}, {820, 380}, {60, 500}, {440, 560}, {960, 480},
        {250, 620}, {750, 540}, {380, 400}, {560, 280}, {140, 440},
        {680, 160}, {850, 300}, {200, 200}, {520, 460}, {350, 80},
    };
    for (int32_t i = 0; i < 20; i++) {
        int32_t sx = star_positions[i][0];
        int32_t sy = star_positions[i][1];
        if (sx < (int32_t)fb.width && sy < (int32_t)(fb.height - WM_TASKBAR_HEIGHT)) {
            fb_putpixel(sx, sy, fb_color(0x40, 0x60, 0x90));
            fb_putpixel(sx + 1, sy, fb_color(0x30, 0x50, 0x80));
            fb_putpixel(sx - 1, sy, fb_color(0x30, 0x50, 0x80));
            fb_putpixel(sx, sy + 1, fb_color(0x30, 0x50, 0x80));
            fb_putpixel(sx, sy - 1, fb_color(0x30, 0x50, 0x80));
        }
    }

    int32_t accent_x = fb.width / 3;
    int32_t accent_y = (fb.height - WM_TASKBAR_HEIGHT) / 2;
    for (int32_t r = 180; r > 0; r -= 4) {
        uint8_t alpha = (uint8_t)(4 * (180 - r) / 180);
        for (int32_t dy = -r; dy < r; dy += 3) {
            for (int32_t dx = -r; dx < r; dx += 3) {
                if (dx * dx + dy * dy < r * r) {
                    int32_t px = accent_x + dx;
                    int32_t py = accent_y + dy;
                    if (px >= 0 && py >= 0 && px < (int32_t)fb.width && py < (int32_t)(fb.height - WM_TASKBAR_HEIGHT)) {
                        uint32_t bg = fb_getpixel(px, py);
                        uint32_t accent = fb_color(0x08, 0x12, 0x28);
                        fb_putpixel(px, py, fb_blend(bg, accent, alpha));
                    }
                }
            }
        }
    }

    fb_rect(0, fb.height - WM_TASKBAR_HEIGHT, fb.width, 1, fb_color(0x12, 0x1E, 0x30));
}

void wm_draw_taskbar(void) {
    int32_t ty = fb.height - WM_TASKBAR_HEIGHT;

    for (int32_t y = ty; y < (int32_t)fb.height; y++) {
        uint8_t t = (uint8_t)((y - ty) * 255 / WM_TASKBAR_HEIGHT);
        uint32_t c = fb_blend(fb_color(0x10, 0x1A, 0x2E), fb_color(0x08, 0x0E, 0x1C), t);
        fb_hline(0, y, fb.width, c);
    }
    fb_rect(0, ty, fb.width, 1, fb_color(0x20, 0x35, 0x55));
    fb_rect(0, ty + 1, fb.width, 1, fb_color(0x18, 0x28, 0x40));

    for (int32_t i = 0; i < wm.taskbar_count; i++) {
        wm_draw_taskbar_entry(&wm.taskbar_entries[i], i);
    }

    wm_draw_clock();
    wm_draw_start_button(0);
}

void wm_draw_start_button(int32_t hovered) {
    int32_t ty = fb.height - WM_TASKBAR_HEIGHT;
    uint32_t bg = hovered ? fb_color(0x1A, 0x30, 0x55) : fb_color(0x0E, 0x16, 0x28);
    uint32_t border = hovered ? WM_COLOR_ACCENT : fb_color(0x1E, 0x30, 0x50);

    fb_rect_rounded(4, ty + 6, 32, 32, 6, bg);
    fb_rect_outline(4, ty + 6, 32, 32, border, 1);

    if (hovered) {
        fb_rect_rounded(4, ty + 6, 32, 32, 6, fb_color(0x15, 0x25, 0x45));
    }

    int32_t bx = 10, by = ty + 12;
    fb_rect(bx, by, 5, 5, 0x3B82F6);
    fb_rect(bx + 7, by, 5, 5, 0xEF4444);
    fb_rect(bx, by + 7, 5, 5, 0x22C55E);
    fb_rect(bx + 7, by + 7, 5, 5, 0xF59E0B);
}

void wm_draw_taskbar_entry(taskbar_entry_t *entry, int32_t index) {
    int32_t ty = fb.height - WM_TASKBAR_HEIGHT;
    int32_t ex = 44 + index * 108;
    int32_t ew = 100;

    uint32_t bg = entry->hovered ? fb_color(0x18, 0x28, 0x42) : fb_color(0x0C, 0x14, 0x24);
    uint32_t border = entry->active ? fb_color(0x30, 0x60, 0xB0) : fb_color(0x18, 0x28, 0x38);

    fb_rect_rounded(ex, ty + 8, ew, 28, 5, bg);
    fb_rect_outline(ex, ty + 8, ew, 28, border, 1);

    if (entry->active) {
        fb_rect(ex + 10, ty + 34, ew - 20, 2, WM_COLOR_ACCENT);
    }

    fb_draw_char(ex + 8, ty + 14, entry->icon_char, entry->icon_color);
    fb_draw_string(ex + 22, ty + 14, entry->label, WM_COLOR_TEXT);
}

void wm_draw_clock(void) {
    int32_t cx = fb.width - 72;
    int32_t ty = fb.height - WM_TASKBAR_HEIGHT;

    uint64_t total_secs = timer_ticks / 100;
    uint32_t mins = (uint32_t)((total_secs / 60) % 60);
    uint32_t hrs = (uint32_t)((total_secs / 3600) % 24);

    char clock_str[6];
    clock_str[0] = '0' + (hrs / 10);
    clock_str[1] = '0' + (hrs % 10);
    clock_str[2] = ':';
    clock_str[3] = '0' + (mins / 10);
    clock_str[4] = '0' + (mins % 10);
    clock_str[5] = 0;

    fb_rect_rounded(cx - 8, ty + 10, 64, 24, 4, fb_color(0x0C, 0x14, 0x24));
    fb_draw_string(cx, ty + 15, clock_str, 0x94A8C8);
}

void wm_draw_all_windows(void) {
    for (int32_t z = wm.next_z_order - 1; z >= 0; z--) {
        for (int32_t i = 0; i < wm.window_count; i++) {
            if (wm.windows[i].visible &&
                wm.windows[i].state != WIN_STATE_MINIMIZED &&
                wm.windows[i].z_order == z) {
                wm_draw_window(&wm.windows[i]);
            }
        }
    }
}

void wm_redraw(void) {
    if (!wm.needs_redraw) return;

    int32_t desktop_h = (int32_t)fb.height - WM_TASKBAR_HEIGHT;
    uint32_t row_bytes = fb.width * 4;

    if (!desktop_cache) {
        desktop_cache_size = (uint32_t)fb.pitch * (uint32_t)desktop_h;
        uint32_t pages = (desktop_cache_size + 4095) / 4096;
        uint8_t *base = 0;
        for (uint32_t i = 0; i < pages; i++) {
            uint8_t *p = (uint8_t *)page_alloc();
            if (!p) break;
            if (i == 0) base = p;
        }
        desktop_cache = base;
    }

    if (desktop_cache && !wm.desktop_dirty) {
        for (int32_t y = 0; y < desktop_h; y++) {
            __builtin_memcpy(fb.backbuf + (uint32_t)y * fb.pitch,
                             desktop_cache + (uint32_t)y * fb.pitch,
                             row_bytes);
        }
    } else {
        wm_draw_desktop();
        if (desktop_cache) {
            for (int32_t y = 0; y < desktop_h; y++) {
                __builtin_memcpy(desktop_cache + (uint32_t)y * fb.pitch,
                                 fb.backbuf + (uint32_t)y * fb.pitch,
                                 row_bytes);
            }
        }
    }

    wm_draw_taskbar();
    for (int32_t i = 0; i < wm.icon_count; i++) {
        if (wm.icons[i].visible) {
            wm_draw_icon(&wm.icons[i]);
        }
    }
    wm_draw_all_windows();
    if (wm.snap_preview) {
        fb_rect_outline(wm.snap_x + 2, wm.snap_y + 2, wm.snap_w - 4, wm.snap_h - 4, fb_color(0x3B, 0x82, 0xF6), 2);
    }
    if (start_menu_open) wm_draw_start_menu();
    if (wm.ctx_menu_open) wm_draw_context_menu();
    mouse_draw_cursor();
    fb_copy_back();
    wm.needs_redraw = 0;
    wm.desktop_dirty = 0;
}

int32_t wm_create_window(const char *title, int32_t x, int32_t y, int32_t w, int32_t h,
                         void (*draw_content)(int32_t, int32_t, int32_t, int32_t)) {
    if (wm.window_count >= WM_MAX_WINDOWS) return -1;

    int32_t id = wm.window_count;
    window_t *win = &wm.windows[id];

    win->id = id;
    int32_t i = 0;
    while (title[i] && i < 63) { win->title[i] = title[i]; i++; }
    win->title[i] = 0;

    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->prev_x = x;
    win->prev_y = y;
    win->prev_w = w;
    win->prev_h = h;
    win->state = WIN_STATE_NORMAL;
    win->focused = 0;
    win->visible = 1;
    win->has_content = (draw_content != 0);
    win->title_color = WM_COLOR_ACCENT;
    win->draw_content = draw_content;
    win->on_keypress = 0;
    win->on_click = 0;
    win->anim_timer = 0;
    win->anim_state = 1;
    win->z_order = wm.next_z_order++;

    wm.window_count++;
    wm_focus_window(id);
    wm.needs_redraw = 1;
    return id;
}

void wm_close_window(int32_t id) {
    if (id < 0 || id >= wm.window_count) return;
    wm.windows[id].visible = 0;
    wm.windows[id].z_order = -1;
    if (wm.focused_window == id) wm.focused_window = -1;
    wm_remove_taskbar_entry(id);
    wm.needs_redraw = 1;
}

void wm_focus_window(int32_t id) {
    if (id < 0 || id >= wm.window_count) return;
    for (int32_t i = 0; i < wm.window_count; i++) {
        wm.windows[i].focused = (i == id) ? 1 : 0;
    }
    wm.focused_window = id;
    wm.needs_redraw = 1;
}

void wm_bring_to_front(int32_t id) {
    if (id < 0 || id >= wm.window_count) return;
    wm.windows[id].z_order = wm.next_z_order++;
    wm_focus_window(id);
}

int32_t wm_find_window_by_title(const char *title) {
    for (int32_t i = 0; i < wm.window_count; i++) {
        if (!wm.windows[i].visible) continue;
        const char *a = wm.windows[i].title;
        const char *b = title;
        while (*a && *b && *a == *b) { a++; b++; }
        if (*a == *b) return i;
    }
    return -1;
}

void wm_minimize_window(int32_t id) {
    if (id < 0 || id >= wm.window_count) return;
    wm.windows[id].state = WIN_STATE_MINIMIZED;
    if (wm.focused_window == id) wm.focused_window = -1;
    wm.needs_redraw = 1;
}

void wm_maximize_window(int32_t id) {
    if (id < 0 || id >= wm.window_count) return;
    window_t *win = &wm.windows[id];
    if (win->state == WIN_STATE_MAXIMIZED) {
        win->x = win->prev_x;
        win->y = win->prev_y;
        win->w = win->prev_w;
        win->h = win->prev_h;
        win->state = WIN_STATE_NORMAL;
    } else {
        win->prev_x = win->x;
        win->prev_y = win->y;
        win->prev_w = win->w;
        win->prev_h = win->h;
        win->x = 0;
        win->y = 0;
        win->w = fb.width;
        win->h = fb.height - WM_TASKBAR_HEIGHT;
        win->state = WIN_STATE_MAXIMIZED;
    }
    wm.needs_redraw = 1;
}

void wm_add_icon(const char *label, char icon_char, uint32_t color, int32_t x, int32_t y, void (*on_click)(void)) {
    if (wm.icon_count >= 12) return;
    desktop_icon_t *icon = &wm.icons[wm.icon_count];
    int32_t i = 0;
    while (label[i] && i < 31) { icon->label[i] = label[i]; i++; }
    icon->label[i] = 0;
    icon->icon_char = icon_char;
    icon->icon_color = color;
    icon->x = x;
    icon->y = y;
    icon->visible = 1;
    icon->hovered = 0;
    icon->on_click = on_click;
    wm.icon_count++;
}

void wm_add_taskbar_entry(const char *label, char icon_char, uint32_t color, int32_t win_id) {
    if (wm.taskbar_count >= WM_MAX_WINDOWS) return;
    taskbar_entry_t *entry = &wm.taskbar_entries[wm.taskbar_count];
    int32_t i = 0;
    while (label[i] && i < 31) { entry->label[i] = label[i]; i++; }
    entry->label[i] = 0;
    entry->icon_char = icon_char;
    entry->icon_color = color;
    entry->active = 1;
    entry->hovered = 0;
    entry->win_id = win_id;
    wm.taskbar_count++;
}

void wm_remove_taskbar_entry(int32_t win_id) {
    for (int32_t i = 0; i < wm.taskbar_count; i++) {
        if (wm.taskbar_entries[i].win_id == win_id) {
            for (int32_t j = i; j < wm.taskbar_count - 1; j++) {
                wm.taskbar_entries[j] = wm.taskbar_entries[j + 1];
            }
            wm.taskbar_count--;
            return;
        }
    }
}

void wm_add_start_item(const char *label, char icon_char, uint32_t color, void (*on_click)(void)) {
    if (wm.start_item_count >= 12) return;
    start_menu_item_t *item = &wm.start_items[wm.start_item_count];
    int32_t i = 0;
    while (label[i] && i < 31) { item->label[i] = label[i]; i++; }
    item->label[i] = 0;
    item->icon_char = icon_char;
    item->icon_color = color;
    item->on_click = on_click;
    wm.start_item_count++;
}

void wm_draw_start_menu(void) {
    int32_t ty = fb.height - WM_TASKBAR_HEIGHT;
    int32_t mw = 230;
    int32_t mh = 16 + wm.start_item_count * 34 + 8;
    int32_t mx = 4;
    int32_t my = ty - mh;

    fb_shadow(mx, my, mw, mh, 10, 20);
    fb_rect_rounded(mx, my, mw, mh, 10, fb_color(0x0E, 0x16, 0x28));
    fb_rect_outline(mx, my, mw, mh, fb_color(0x20, 0x35, 0x55), 1);

    fb_rect_rounded(mx + 6, my + 6, mw - 12, 26, 5, fb_color(0x14, 0x22, 0x38));
    fb_draw_string(mx + 14, my + 11, "Qiki OS", WM_COLOR_ACCENT);

    for (int32_t i = 0; i < wm.start_item_count; i++) {
        start_menu_item_t *item = &wm.start_items[i];
        int32_t iy = my + 40 + i * 34;

        fb_rect_rounded(mx + 8, iy, mw - 16, 28, 5, fb_color(0x10, 0x1C, 0x30));
        fb_draw_char(mx + 16, iy + 7, item->icon_char, item->icon_color);
        fb_draw_string(mx + 34, iy + 7, item->label, WM_COLOR_TEXT);
    }
}

void wm_show_context_menu(int32_t x, int32_t y, void (*on_refresh)(void), void (*on_terminal)(void), void (*on_about)(void)) {
    wm.ctx_menu_open = 1;
    wm.ctx_menu_x = x;
    wm.ctx_menu_y = y;
    wm.ctx_item_count = 0;

    static const char *labels[] = {"Refresh Desktop", "Open Terminal", "About Qiki OS"};
    void (*callbacks[])(void) = {on_refresh, on_terminal, on_about};
    for (int32_t i = 0; i < 3; i++) {
        if (wm.ctx_item_count >= 8) break;
        ctx_menu_item_t *item = &wm.ctx_items[wm.ctx_item_count];
        int32_t j = 0;
        while (labels[i][j] && j < 31) { item->label[j] = labels[i][j]; j++; }
        item->label[j] = 0;
        item->on_click = callbacks[i];
        wm.ctx_item_count++;
    }
    wm.needs_redraw = 1;
}

void wm_hide_context_menu(void) {
    wm.ctx_menu_open = 0;
    wm.ctx_item_count = 0;
    wm.needs_redraw = 1;
}

void wm_draw_context_menu(void) {
    int32_t mw = 190;
    int32_t mh = 12 + wm.ctx_item_count * 30;
    int32_t mx = wm.ctx_menu_x;
    int32_t my = wm.ctx_menu_y;

    if (mx + mw > (int32_t)fb.width) mx = fb.width - mw;
    if (my + mh > (int32_t)(fb.height - WM_TASKBAR_HEIGHT)) my = fb.height - WM_TASKBAR_HEIGHT - mh;

    fb_shadow(mx, my, mw, mh, 8, 14);
    fb_rect_rounded(mx, my, mw, mh, 8, fb_color(0x0E, 0x16, 0x28));
    fb_rect_outline(mx, my, mw, mh, fb_color(0x20, 0x35, 0x55), 1);

    for (int32_t i = 0; i < wm.ctx_item_count; i++) {
        int32_t iy = my + 6 + i * 30;
        fb_rect_rounded(mx + 6, iy, mw - 12, 26, 4, fb_color(0x14, 0x22, 0x38));
        fb_draw_string(mx + 16, iy + 6, wm.ctx_items[i].label, WM_COLOR_TEXT);
    }
}

void wm_handle_ctx_click(int32_t mx, int32_t my) {
    int32_t mw = 190;
    int32_t mh = 12 + wm.ctx_item_count * 30;
    int32_t menu_x = wm.ctx_menu_x;
    int32_t menu_y = wm.ctx_menu_y;

    if (mx < menu_x || mx >= menu_x + mw || my < menu_y || my >= menu_y + mh) {
        wm_hide_context_menu();
        return;
    }

    for (int32_t i = 0; i < wm.ctx_item_count; i++) {
        int32_t iy = menu_y + 6 + i * 30;
        if (mx >= menu_x + 6 && mx < menu_x + mw - 6 && my >= iy && my < iy + 26) {
            if (wm.ctx_items[i].on_click) wm.ctx_items[i].on_click();
            wm_hide_context_menu();
            return;
        }
    }
    wm_hide_context_menu();
}

static void wm_draw_titlebar_at(window_t *win, int32_t x, int32_t y, int32_t w);

void wm_draw_window(window_t *win) {
    int32_t x = win->x, y = win->y, w = win->w, h = win->h;

    if (win->anim_state == 1) {
        win->anim_timer++;
        if (win->anim_timer >= 8) {
            win->anim_state = 0;
            win->anim_timer = 0;
        }
        int32_t progress = win->anim_timer;
        int32_t scale_pct = 85 + progress * 2;
        int32_t new_w = w * scale_pct / 100;
        int32_t new_h = h * scale_pct / 100;
        x = win->x + (w - new_w) / 2;
        y = win->y + (h - new_h) / 2;
        w = new_w;
        h = new_h;
    }

    fb_shadow(x + 3, y + 3, w, h, 10, 18);

    fb_rect_rounded(x, y, w, h, 10, fb_color(0x0E, 0x14, 0x22));
    uint32_t border = win->focused ? fb_color(0x28, 0x48, 0x78) : fb_color(0x18, 0x28, 0x3C);
    fb_rect_outline(x, y, w, h, border, 1);

    wm_draw_titlebar_at(win, x, y, w);

    if (win->has_content && win->draw_content) {
        win->draw_content(x + 1, y + WM_TITLEBAR_HEIGHT + 1, w - 2, h - WM_TITLEBAR_HEIGHT - 1);
    } else {
        fb_rect(x + 1, y + WM_TITLEBAR_HEIGHT + 1, w - 2, h - WM_TITLEBAR_HEIGHT - 1, WM_COLOR_WINDOW_BG2);
    }
}

void wm_draw_titlebar(window_t *win) {
    wm_draw_titlebar_at(win, win->x, win->y, win->w);
}

static void wm_draw_titlebar_at(window_t *win, int32_t x, int32_t y, int32_t w) {
    uint32_t title_bg = win->focused ? fb_color(0x12, 0x20, 0x3A) : fb_color(0x0E, 0x16, 0x28);
    fb_rect_rounded(x + 1, y + 1, w - 2, WM_TITLEBAR_HEIGHT, 9, title_bg);
    fb_rect(x + 1, y + WM_TITLEBAR_HEIGHT - 2, w - 2, 4, title_bg);

    fb_draw_string(x + 14, y + 10, win->title, WM_COLOR_TEXT);

    int32_t close_bx = x + w - 28;
    int32_t close_by = y + 7;
    uint32_t close_bg = win->focused ? fb_color(0x88, 0x22, 0x22) : fb_color(0x40, 0x40, 0x40);
    fb_rect_rounded(close_bx, close_by, 16, 16, 4, close_bg);
    draw_window_close_x(close_bx + 8, close_by + 8, 0xFFCCCC);

    int32_t min_bx = x + w - 48;
    int32_t min_by = y + 7;
    uint32_t min_bg = win->focused ? fb_color(0x6B, 0x5A, 0x15) : fb_color(0x40, 0x40, 0x40);
    fb_rect_rounded(min_bx, min_by, 16, 16, 4, min_bg);
    draw_window_minimize_line(min_bx + 8, min_by + 8, 0xFFF0CC);

    int32_t max_bx = x + w - 68;
    int32_t max_by = y + 7;
    uint32_t max_bg = win->focused ? fb_color(0x18, 0x60, 0x30) : fb_color(0x40, 0x40, 0x40);
    fb_rect_rounded(max_bx, max_by, 16, 16, 4, max_bg);
    draw_window_maximize_square(max_bx + 8, max_by + 8, 0xCCFFCC);
}

void wm_draw_close_button(window_t *win) {
    int32_t bx = win->x + win->w - 28;
    int32_t by = win->y + 7;
    uint32_t bg = win->focused ? fb_color(0x88, 0x22, 0x22) : fb_color(0x40, 0x40, 0x40);
    fb_rect_rounded(bx, by, 16, 16, 4, bg);
    draw_window_close_x(bx + 8, by + 8, 0xFFCCCC);
}

void wm_draw_minimize_button(window_t *win) {
    int32_t bx = win->x + win->w - 48;
    int32_t by = win->y + 7;
    uint32_t bg = win->focused ? fb_color(0x6B, 0x5A, 0x15) : fb_color(0x40, 0x40, 0x40);
    fb_rect_rounded(bx, by, 16, 16, 4, bg);
    draw_window_minimize_line(bx + 8, by + 8, 0xFFF0CC);
}

void wm_draw_maximize_button(window_t *win) {
    int32_t bx = win->x + win->w - 68;
    int32_t by = win->y + 7;
    uint32_t bg = win->focused ? fb_color(0x18, 0x60, 0x30) : fb_color(0x40, 0x40, 0x40);
    fb_rect_rounded(bx, by, 16, 16, 4, bg);
    draw_window_maximize_square(bx + 8, by + 8, 0xCCFFCC);
}

void wm_draw_icon(desktop_icon_t *icon) {
    uint32_t bg = icon->hovered ? fb_color(0x16, 0x2A, 0x48) : fb_color(0x0C, 0x16, 0x28);
    fb_rect_rounded(icon->x, icon->y, 64, 64, 12, bg);
    fb_rect_outline(icon->x, icon->y, 64, 64, fb_color(0x1A, 0x2C, 0x48), 1);

    if (icon->hovered) {
        fb_rect_rounded(icon->x, icon->y, 64, 64, 12, fb_color(0x12, 0x22, 0x3C));
    }

    uint32_t inner_bg = fb_color(0x14, 0x24, 0x40);
    fb_rect_rounded(icon->x + 14, icon->y + 8, 36, 36, 8, inner_bg);
    fb_draw_char(icon->x + 24, icon->y + 16, icon->icon_char, icon->icon_color);

    int32_t tw = fb_string_width(icon->label);
    fb_draw_string(icon->x + 32 - tw / 2, icon->y + 52, icon->label, WM_COLOR_TEXT);
}

int32_t wm_hit_test(int32_t mx, int32_t my) {
    int32_t best = -1;
    int32_t best_z = -1;
    for (int32_t i = 0; i < wm.window_count; i++) {
        window_t *win = &wm.windows[i];
        if (!win->visible || win->state == WIN_STATE_MINIMIZED) continue;
        if (mx >= win->x && mx < win->x + win->w && my >= win->y && my < win->y + win->h) {
            if (win->z_order > best_z) {
                best_z = win->z_order;
                best = i;
            }
        }
    }
    return best;
}

int32_t wm_find_icon_at(int32_t mx, int32_t my) {
    for (int32_t i = 0; i < wm.icon_count; i++) {
        desktop_icon_t *icon = &wm.icons[i];
        if (!icon->visible) continue;
        if (mx >= icon->x && mx < icon->x + 64 && my >= icon->y && my < icon->y + 82) {
            return i;
        }
    }
    return -1;
}

void wm_handle_click(int32_t mx, int32_t my, uint8_t button) {
    int32_t ty = fb.height - WM_TASKBAR_HEIGHT;

    if (wm.ctx_menu_open) {
        wm_handle_ctx_click(mx, my);
        return;
    }

    if (button == 1) {
        wm_show_context_menu(mx, my, wm.ctx_on_refresh, wm.ctx_on_terminal, wm.ctx_on_about);
        return;
    }

    if (my >= ty) {
        if (mx < 38) {
            start_menu_open = !start_menu_open;
            wm.needs_redraw = 1;
            return;
        }
        if (start_menu_open) {
            int32_t smw = 230;
            int32_t smh = 16 + wm.start_item_count * 34 + 8;
            int32_t smx = 4;
            int32_t smy = ty - smh;
            if (mx >= smx && mx < smx + smw && my >= smy && my < ty) {
                for (int32_t i = 0; i < wm.start_item_count; i++) {
                    int32_t iy = smy + 40 + i * 34;
                    if (mx >= smx + 8 && mx < smx + smw - 8 && my >= iy && my < iy + 28) {
                        if (wm.start_items[i].on_click) wm.start_items[i].on_click();
                        start_menu_open = 0;
                        wm.needs_redraw = 1;
                        return;
                    }
                }
                return;
            }
            start_menu_open = 0;
            wm.needs_redraw = 1;
            return;
        }
        for (int32_t i = 0; i < wm.taskbar_count; i++) {
            int32_t ex = 44 + i * 108;
            if (mx >= ex && mx < ex + 100) {
                int32_t wid = wm.taskbar_entries[i].win_id;
                if (wid >= 0 && wid < wm.window_count) {
                    if (wm.windows[wid].state == WIN_STATE_MINIMIZED) {
                        wm.windows[wid].state = WIN_STATE_NORMAL;
                        wm_bring_to_front(wid);
                    } else if (wm.focused_window == wid) {
                        wm_minimize_window(wid);
                    } else {
                        wm_bring_to_front(wid);
                    }
                }
                wm.needs_redraw = 1;
                return;
            }
        }
        return;
    }

    if (start_menu_open) {
        start_menu_open = 0;
        wm.needs_redraw = 1;
        return;
    }

    int32_t wid = wm_hit_test(mx, my);
    if (wid >= 0) {
        window_t *win = &wm.windows[wid];
        wm_bring_to_front(wid);

        if (my < win->y + WM_TITLEBAR_HEIGHT) {
            if (mx >= win->x + win->w - 28 && mx < win->x + win->w - 12 &&
                my >= win->y + 7 && my < win->y + 23) {
                wm_close_window(wid);
                return;
            }
            if (mx >= win->x + win->w - 48 && mx < win->x + win->w - 32 &&
                my >= win->y + 7 && my < win->y + 23) {
                wm_minimize_window(wid);
                return;
            }
            if (mx >= win->x + win->w - 68 && mx < win->x + win->w - 52 &&
                my >= win->y + 7 && my < win->y + 23) {
                wm_maximize_window(wid);
                return;
            }

            wm.drag_window = wid;
            wm.dragging = 1;
            wm.drag_offset_x = mx - win->x;
            wm.drag_offset_y = my - win->y;
        } else {
            resize_edge_t edge = wm_check_resize_edge(mx, my, wid);
            if (edge != RESIZE_NONE) {
                wm.resize_window = wid;
                wm.resize_edge = edge;
                wm.resize_start_x = mx;
                wm.resize_start_y = my;
                wm.resize_orig_x = win->x;
                wm.resize_orig_y = win->y;
                wm.resize_orig_w = win->w;
                wm.resize_orig_h = win->h;
            } else if (win->on_click) {
                int32_t cx = mx - win->x - 1;
                int32_t cy = my - win->y - WM_TITLEBAR_HEIGHT - 1;
                win->on_click(cx, cy);
            }
        }
        return;
    }

    int32_t iid = wm_find_icon_at(mx, my);
    if (iid >= 0) {
        desktop_icon_t *icon = &wm.icons[iid];
        if (icon->on_click) icon->on_click();
        wm.needs_redraw = 1;
        return;
    }

    for (int32_t i = 0; i < wm.window_count; i++) {
        wm.windows[i].focused = 0;
    }
    wm.focused_window = -1;
    wm.needs_redraw = 1;
}

void wm_handle_drag(int32_t mx, int32_t my) {
    if (!wm.dragging || wm.drag_window < 0) return;
    window_t *win = &wm.windows[wm.drag_window];
    win->x = mx - wm.drag_offset_x;
    win->y = my - wm.drag_offset_y;
    if (win->x < 0) win->x = 0;
    if (win->y < 0) win->y = 0;
    if (win->x + win->w > (int32_t)fb.width) win->x = fb.width - win->w;
    if (win->y + win->h > (int32_t)(fb.height - WM_TASKBAR_HEIGHT)) win->y = fb.height - WM_TASKBAR_HEIGHT - win->h;

    int32_t screen_w = fb.width;
    int32_t screen_h = fb.height - WM_TASKBAR_HEIGHT;
    int32_t snap_zone = 8;
    wm.snap_preview = 0;

    if (mx <= snap_zone) {
        wm.snap_x = 0; wm.snap_y = 0;
        wm.snap_w = screen_w / 2; wm.snap_h = screen_h;
        wm.snap_preview = 1;
    } else if (mx >= screen_w - snap_zone) {
        wm.snap_x = screen_w / 2; wm.snap_y = 0;
        wm.snap_w = screen_w / 2; wm.snap_h = screen_h;
        wm.snap_preview = 1;
    } else if (my <= snap_zone) {
        wm.snap_x = 0; wm.snap_y = 0;
        wm.snap_w = screen_w; wm.snap_h = screen_h;
        wm.snap_preview = 1;
    }

    wm.needs_redraw = 1;
}

void wm_handle_drag_end(void) {
    if (wm.dragging && wm.drag_window >= 0 && wm.snap_preview) {
        window_t *win = &wm.windows[wm.drag_window];
        win->prev_x = win->x;
        win->prev_y = win->y;
        win->prev_w = win->w;
        win->prev_h = win->h;
        win->x = wm.snap_x;
        win->y = wm.snap_y;
        win->w = wm.snap_w;
        win->h = wm.snap_h;
        win->state = WIN_STATE_MAXIMIZED;
    }
    wm.dragging = 0;
    wm.drag_window = -1;
    wm.snap_preview = 0;
    wm.needs_redraw = 1;
}

resize_edge_t wm_check_resize_edge(int32_t mx, int32_t my, int32_t win_id) {
    if (win_id < 0 || win_id >= wm.window_count) return RESIZE_NONE;
    window_t *win = &wm.windows[win_id];
    if (win->state == WIN_STATE_MAXIMIZED) return RESIZE_NONE;

    int32_t zone = 5;
    int32_t x = win->x, y = win->y, w = win->w, h = win->h;

    int32_t on_left = (mx >= x && mx < x + zone);
    int32_t on_right = (mx >= x + w - zone && mx < x + w);
    int32_t on_top = (my >= y && my < y + zone);
    int32_t on_bottom = (my >= y + h - zone && my < y + h);

    if (on_top && on_left) return RESIZE_TOP_LEFT;
    if (on_top && on_right) return RESIZE_TOP_RIGHT;
    if (on_bottom && on_left) return RESIZE_BOTTOM_LEFT;
    if (on_bottom && on_right) return RESIZE_BOTTOM_RIGHT;
    if (on_left) return RESIZE_LEFT;
    if (on_right) return RESIZE_RIGHT;
    if (on_top) return RESIZE_TOP;
    if (on_bottom) return RESIZE_BOTTOM;
    return RESIZE_NONE;
}

void wm_handle_resize(int32_t mx, int32_t my) {
    if (wm.resize_window < 0 || wm.resize_edge == RESIZE_NONE) return;
    window_t *win = &wm.windows[wm.resize_window];

    int32_t dx = mx - wm.resize_start_x;
    int32_t dy = my - wm.resize_start_y;
    int32_t nx = wm.resize_orig_x;
    int32_t ny = wm.resize_orig_y;
    int32_t nw = wm.resize_orig_w;
    int32_t nh = wm.resize_orig_h;

    if (wm.resize_edge == RESIZE_LEFT || wm.resize_edge == RESIZE_TOP_LEFT || wm.resize_edge == RESIZE_BOTTOM_LEFT) {
        nx = wm.resize_orig_x + dx;
        nw = wm.resize_orig_w - dx;
    }
    if (wm.resize_edge == RESIZE_RIGHT || wm.resize_edge == RESIZE_TOP_RIGHT || wm.resize_edge == RESIZE_BOTTOM_RIGHT) {
        nw = wm.resize_orig_w + dx;
    }
    if (wm.resize_edge == RESIZE_TOP || wm.resize_edge == RESIZE_TOP_LEFT || wm.resize_edge == RESIZE_TOP_RIGHT) {
        ny = wm.resize_orig_y + dy;
        nh = wm.resize_orig_h - dy;
    }
    if (wm.resize_edge == RESIZE_BOTTOM || wm.resize_edge == RESIZE_BOTTOM_LEFT || wm.resize_edge == RESIZE_BOTTOM_RIGHT) {
        nh = wm.resize_orig_h + dy;
    }

    if (nw < WM_MIN_WIDTH) { nw = WM_MIN_WIDTH; }
    if (nh < WM_MIN_HEIGHT) { nh = WM_MIN_HEIGHT; }
    if (nx < 0) { nw += nx; nx = 0; }
    if (ny < 0) { nh += ny; ny = 0; }

    win->x = nx;
    win->y = ny;
    win->w = nw;
    win->h = nh;
    wm.needs_redraw = 1;
}

void wm_handle_resize_end(void) {
    wm.resize_window = -1;
    wm.resize_edge = RESIZE_NONE;
}

void wm_set_start_menu_open(uint8_t open) {
    start_menu_open = open;
}

uint8_t wm_is_start_menu_open(void) {
    return start_menu_open;
}

void wm_forward_keypress(char c, uint8_t scancode) {
    if (wm.focused_window >= 0 && wm.focused_window < wm.window_count) {
        window_t *win = &wm.windows[wm.focused_window];
        if (win->visible && win->on_keypress) {
            win->on_keypress(c, scancode);
            wm.needs_redraw = 1;
        }
    }
}
