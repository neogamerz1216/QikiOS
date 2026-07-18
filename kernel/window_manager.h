#ifndef WINDOW_MANAGER_H
#define WINDOW_MANAGER_H

#include <stdint.h>
#include "framebuffer.h"

#define WM_MAX_WINDOWS 16
#define WM_TASKBAR_HEIGHT 44
#define WM_TITLEBAR_HEIGHT 30
#define WM_MIN_WIDTH 200
#define WM_MIN_HEIGHT 150

#define WM_COLOR_BG          0x0A0E1A
#define WM_COLOR_TASKBAR     0x0F1525
#define WM_COLOR_TASKBAR_TOP 0x1A2840
#define WM_COLOR_TITLEBAR    0x141E32
#define WM_COLOR_TITLEBAR_F  0x2563EB
#define WM_COLOR_WINDOW_BG   0x111827
#define WM_COLOR_WINDOW_BG2  0x0D1220
#define WM_COLOR_TEXT         0xF1F5F9
#define WM_COLOR_TEXT_DIM     0x6B7FA0
#define WM_COLOR_ACCENT       0x3B82F6
#define WM_COLOR_ACCENT_HOVER 0x60A5FA
#define WM_COLOR_BORDER       0x1E3050
#define WM_COLOR_CLOSE        0xEF4444
#define WM_COLOR_CLOSE_HOVER  0xF87171
#define WM_COLOR_MINIMIZE     0xF59E0B
#define WM_COLOR_MAXIMIZE     0x22C55E
#define WM_COLOR_ICON_BG      0x111D30
#define WM_COLOR_ICON_HOVER   0x1A3050
#define WM_COLOR_SHADOW       0x000000
#define WM_COLOR_START_MENU   0x111827
#define WM_COLOR_CTX_MENU     0x111827

typedef enum {
    WIN_STATE_NORMAL = 0,
    WIN_STATE_MINIMIZED,
    WIN_STATE_MAXIMIZED,
} win_state_t;

typedef enum {
    RESIZE_NONE = 0,
    RESIZE_LEFT,
    RESIZE_RIGHT,
    RESIZE_TOP,
    RESIZE_BOTTOM,
    RESIZE_TOP_LEFT,
    RESIZE_TOP_RIGHT,
    RESIZE_BOTTOM_LEFT,
    RESIZE_BOTTOM_RIGHT,
} resize_edge_t;

typedef struct {
    int32_t id;
    char title[64];
    int32_t x, y, w, h;
    int32_t prev_x, prev_y, prev_w, prev_h;
    win_state_t state;
    uint8_t focused;
    uint8_t visible;
    uint8_t has_content;
    uint32_t title_color;
    int32_t z_order;
    void (*draw_content)(int32_t x, int32_t y, int32_t w, int32_t h);
    void (*on_keypress)(char c, uint8_t scancode);
    void (*on_click)(int32_t cx, int32_t cy);

    int32_t anim_timer;
    uint8_t anim_state;
} window_t;

typedef struct {
    int32_t x, y;
    char label[32];
    char icon_char;
    uint32_t icon_color;
    uint8_t visible;
    uint8_t hovered;
    void (*on_click)(void);
} desktop_icon_t;

typedef struct {
    char label[32];
    char icon_char;
    uint32_t icon_color;
    uint8_t active;
    uint8_t hovered;
    int32_t win_id;
} taskbar_entry_t;

typedef struct {
    char label[32];
    char icon_char;
    uint32_t icon_color;
    void (*on_click)(void);
} start_menu_item_t;

typedef struct {
    char label[32];
    void (*on_click)(void);
} ctx_menu_item_t;

typedef struct {
    window_t windows[WM_MAX_WINDOWS];
    int32_t window_count;
    int32_t focused_window;
    int32_t drag_window;
    int32_t drag_offset_x, drag_offset_y;
    uint8_t dragging;

    int32_t resize_window;
    resize_edge_t resize_edge;
    int32_t resize_start_x, resize_start_y;
    int32_t resize_orig_x, resize_orig_y, resize_orig_w, resize_orig_h;

    desktop_icon_t icons[12];
    int32_t icon_count;

    taskbar_entry_t taskbar_entries[WM_MAX_WINDOWS];
    int32_t taskbar_count;

    start_menu_item_t start_items[12];
    int32_t start_item_count;

    ctx_menu_item_t ctx_items[8];
    int32_t ctx_item_count;
    uint8_t ctx_menu_open;
    int32_t ctx_menu_x, ctx_menu_y;

    int32_t next_z_order;

    uint8_t needs_redraw;
    uint8_t desktop_dirty;

    void (*ctx_on_refresh)(void);
    void (*ctx_on_terminal)(void);
    void (*ctx_on_about)(void);

    uint8_t snap_preview;
    int32_t snap_x, snap_y, snap_w, snap_h;
} wm_state_t;

extern wm_state_t wm;

void wm_init(void);
void wm_draw_desktop(void);
void wm_draw_taskbar(void);
void wm_draw_all_windows(void);
void wm_redraw(void);

int32_t wm_create_window(const char *title, int32_t x, int32_t y, int32_t w, int32_t h,
                         void (*draw_content)(int32_t, int32_t, int32_t, int32_t));
void wm_close_window(int32_t id);
void wm_focus_window(int32_t id);
void wm_minimize_window(int32_t id);
void wm_maximize_window(int32_t id);
void wm_bring_to_front(int32_t id);

void wm_add_icon(const char *label, char icon_char, uint32_t color, int32_t x, int32_t y, void (*on_click)(void));
void wm_add_taskbar_entry(const char *label, char icon_char, uint32_t color, int32_t win_id);
void wm_remove_taskbar_entry(int32_t win_id);

void wm_add_start_item(const char *label, char icon_char, uint32_t color, void (*on_click)(void));
void wm_draw_start_menu(void);

void wm_show_context_menu(int32_t x, int32_t y, void (*on_refresh)(void), void (*on_terminal)(void), void (*on_about)(void));
void wm_hide_context_menu(void);
void wm_draw_context_menu(void);
void wm_handle_ctx_click(int32_t mx, int32_t my);

void wm_handle_click(int32_t mx, int32_t my, uint8_t button);
void wm_handle_drag(int32_t mx, int32_t my);
void wm_handle_drag_end(void);

resize_edge_t wm_check_resize_edge(int32_t mx, int32_t my, int32_t win_id);
void wm_handle_resize(int32_t mx, int32_t my);
void wm_handle_resize_end(void);

void wm_draw_window(window_t *win);
void wm_draw_titlebar(window_t *win);
void wm_draw_close_button(window_t *win);
void wm_draw_minimize_button(window_t *win);
void wm_draw_maximize_button(window_t *win);

void wm_draw_icon(desktop_icon_t *icon);
void wm_draw_taskbar_entry(taskbar_entry_t *entry, int32_t index);

void wm_draw_clock(void);
void wm_draw_start_button(int32_t hovered);

int32_t wm_hit_test(int32_t mx, int32_t my);
int32_t wm_find_icon_at(int32_t mx, int32_t my);

void wm_set_start_menu_open(uint8_t open);
uint8_t wm_is_start_menu_open(void);

void wm_forward_keypress(char c, uint8_t scancode);

int32_t mouse_get_x(void);
int32_t mouse_get_y(void);
void mouse_draw_cursor(void);

int32_t wm_find_window_by_title(const char *title);

#endif
