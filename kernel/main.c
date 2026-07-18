#include <stdint.h>
#include <stddef.h>
#include "framebuffer.h"
#include "window_manager.h"
#include "io.h"

extern void serial_init();
extern void gdt_init();
extern void idt_init();
extern void pic_init();
extern void mem_init(uint64_t total_mem);
extern void heap_init();
extern void scheduler_init();
extern void mouse_init();
extern void printk(const char *fmt, ...);

extern uint64_t timer_ticks;

static void refresh_desktop(void) {
    wm.needs_redraw = 1;
}

#define TERM_MAX_LINES 128
#define TERM_LINE_LEN  80
#define TERM_INPUT_LEN 72

typedef struct {
    char lines[TERM_MAX_LINES][TERM_LINE_LEN];
    int32_t line_count;
    int32_t scroll_y;
    char input_buf[TERM_INPUT_LEN + 1];
    int32_t input_len;
    int32_t input_cursor;
    uint8_t has_focus;
} terminal_state_t;

static terminal_state_t term = {0};

static void term_init(void) {
    for (int32_t i = 0; i < TERM_MAX_LINES; i++)
        for (int32_t j = 0; j < TERM_LINE_LEN; j++)
            term.lines[i][j] = 0;
    term.line_count = 0;
    term.scroll_y = 0;
    term.input_len = 0;
    term.input_cursor = 0;
    term.has_focus = 0;

    const char *welcome1 = "Qiki OS Terminal v0.4";
    const char *welcome2 = "Type 'help' for available commands.";
    int32_t k;
    k = 0;
    while (welcome1[k] && k < TERM_LINE_LEN - 1) { term.lines[0][k] = welcome1[k]; k++; }
    term.lines[0][k] = 0;
    k = 0;
    while (welcome2[k] && k < TERM_LINE_LEN - 1) { term.lines[1][k] = welcome2[k]; k++; }
    term.lines[1][k] = 0;
    term.lines[2][0] = 0;
    term.line_count = 3;
}

static void term_print(const char *s) {
    if (term.line_count >= TERM_MAX_LINES) return;
    int32_t k = 0;
    while (s[k] && k < TERM_LINE_LEN - 1) {
        term.lines[term.line_count][k] = s[k];
        k++;
    }
    term.lines[term.line_count][k] = 0;
    term.line_count++;
}

static void term_clear(void) {
    for (int32_t i = 0; i < TERM_MAX_LINES; i++)
        term.lines[i][0] = 0;
    term.line_count = 0;
    term.scroll_y = 0;
}

static int32_t term_strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *(unsigned char*)a - *(unsigned char*)b;
}

static void term_str_copy(char *dst, const char *src, int32_t max) {
    int32_t i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static void term_handle_command(void) {
    char cmd_line[TERM_INPUT_LEN + 1];
    term_str_copy(cmd_line, term.input_buf, TERM_INPUT_LEN + 1);

    char prompt[TERM_LINE_LEN];
    int32_t pi = 0;
    const char *pfx = "qiki> ";
    while (pfx[pi]) { prompt[pi] = pfx[pi]; pi++; }
    int32_t ci = 0;
    while (cmd_line[ci] && pi < TERM_LINE_LEN - 1) { prompt[pi] = cmd_line[ci]; pi++; ci++; }
    prompt[pi] = 0;
    term_print(prompt);

    if (cmd_line[0] == 0) return;

    if (term_strcmp(cmd_line, "help") == 0) {
        term_print("Available commands:");
        term_print("  help     - Show this help message");
        term_print("  clear    - Clear the terminal");
        term_print("  echo     - Print text");
        term_print("  uname    - System information");
        term_print("  uptime   - System uptime");
        term_print("  date     - Current date/time");
        term_print("  ls       - List files");
        term_print("  cat      - Show file contents");
        term_print("  mem      - Memory information");
        term_print("  reboot   - Reboot the system");
    } else if (term_strcmp(cmd_line, "clear") == 0) {
        term_clear();
    } else if (term_strcmp(cmd_line, "uname") == 0) {
        term_print("Qiki OS 0.4 x86_64 Bare Metal");
    } else if (term_strcmp(cmd_line, "uptime") == 0) {
        uint64_t ticks = timer_ticks;
        uint64_t seconds = ticks / 100;
        uint64_t minutes = seconds / 60;
        uint64_t hours = minutes / 60;
        seconds %= 60;
        minutes %= 60;
        char buf[32] = "Uptime: 00:00:00";
        buf[9] = '0' + (hours / 10);
        buf[10] = '0' + (hours % 10);
        buf[12] = '0' + (minutes / 10);
        buf[13] = '0' + (minutes % 10);
        buf[15] = '0' + (seconds / 10);
        buf[16] = '0' + (seconds % 10);
        buf[17] = 0;
        term_print(buf);
    } else if (term_strcmp(cmd_line, "date") == 0) {
        term_print("2026-07-18 12:00:00 UTC");
        term_print("(Real-time clock not yet implemented)");
    } else if (term_strcmp(cmd_line, "mem") == 0) {
        term_print("Total: 64 MB");
        term_print("Used:  ~2 MB");
        term_print("Free:  ~62 MB");
    } else if (term_strcmp(cmd_line, "ls") == 0) {
        term_print("Desktop/   Documents/   Downloads/");
        term_print("readme.txt   config.sys   kernel.bin");
    } else if (term_strcmp(cmd_line, "reboot") == 0) {
        term_print("Rebooting...");
        outb(0x64, 0xFE);
    } else if (cmd_line[0] == 'e' && cmd_line[1] == 'c' && cmd_line[2] == 'h' && cmd_line[3] == 'o' && cmd_line[4] == ' ') {
        term_print(cmd_line + 5);
    } else if (cmd_line[0] == 'c' && cmd_line[1] == 'a' && cmd_line[2] == 't' && cmd_line[3] == ' ') {
        const char *fname = cmd_line + 4;
        if (term_strcmp(fname, "readme.txt") == 0) {
            term_print("Welcome to Qiki OS!");
            term_print("This is a hobby operating system");
            term_print("built from scratch in C and ASM.");
        } else if (term_strcmp(fname, "config.sys") == 0) {
            term_print("BOOT=kernel.bin");
            term_print("VIDEO=1024x768x32");
            term_print("MEM=64M");
        } else {
            term_print("cat: file not found");
        }
    } else {
        term_print("Unknown command. Type 'help' for list.");
    }
}

static void term_keypress(char c, uint8_t scancode) {
    (void)scancode;
    if (c == '\n') {
        term.input_buf[term.input_len] = 0;
        term_handle_command();
        term.input_len = 0;
        term.input_cursor = 0;
    } else if (c == '\b') {
        if (term.input_cursor > 0) {
            for (int32_t i = term.input_cursor - 1; i < term.input_len - 1; i++) {
                term.input_buf[i] = term.input_buf[i + 1];
            }
            term.input_len--;
            term.input_cursor--;
        }
    } else if (c >= 32 && c < 127 && term.input_len < TERM_INPUT_LEN) {
        for (int32_t i = term.input_len; i > term.input_cursor; i--) {
            term.input_buf[i] = term.input_buf[i - 1];
        }
        term.input_buf[term.input_cursor] = c;
        term.input_len++;
        term.input_cursor++;
    }
}

static void draw_terminal_content(int32_t x, int32_t y, int32_t w, int32_t h) {
    fb_rect(x, y, w, h, 0x0C0C0C);

    int32_t line_height = 12;
    int32_t visible_lines = (h - 4) / line_height;
    int32_t total_lines = term.line_count + 1;
    if (total_lines > visible_lines) term.scroll_y = total_lines - visible_lines;
    if (term.scroll_y < 0) term.scroll_y = 0;

    for (int32_t i = 0; i < visible_lines; i++) {
        int32_t line_idx = term.scroll_y + i;
        int32_t ly = y + 2 + i * line_height;

        if (line_idx < term.line_count) {
            fb_draw_string(x + 6, ly, term.lines[line_idx], 0x33FF33);
        } else if (line_idx == term.line_count) {
            const char *prompt = "qiki> ";
            fb_draw_string(x + 6, ly, prompt, 0x33FF33);
            fb_draw_string(x + 6 + 6 * 8, ly, term.input_buf, 0xCCFFCC);

            if ((timer_ticks % 20) < 10) {
                int32_t cx = x + 6 + (6 + term.input_cursor) * 8;
                fb_rect(cx, ly, 7, line_height, 0x33FF33);
            }
        }
    }

    fb_rect(x, y + h - 1, w, 1, 0x33FF33);
}

static void open_terminal(void) {
    int32_t existing = wm_find_window_by_title("Terminal");
    if (existing >= 0) {
        wm_bring_to_front(existing);
        return;
    }
    term_init();
    int32_t id = wm_create_window("Terminal", 120, 60, 540, 380, draw_terminal_content);
    if (id >= 0) {
        wm.windows[id].on_keypress = term_keypress;
        wm.windows[id].title_color = 0x33FF33;
        wm_add_taskbar_entry("Terminal", '>', 0x33FF33, id);
    }
}

static void draw_about_content(int32_t x, int32_t y, int32_t w, int32_t h) {
    fb_rect(x, y, w, h, WM_COLOR_WINDOW_BG2);

    fb_rect_rounded(x + 16, y + 14, 40, 40, 8, WM_COLOR_ACCENT);
    fb_draw_char(x + 26, y + 22, 'Q', 0xFFFFFF);

    fb_draw_string_scaled(x + 64, y + 20, "Qiki OS", WM_COLOR_ACCENT, 2);
    fb_draw_string(x + 64, y + 40, "Version 0.4", WM_COLOR_TEXT_DIM);

    fb_hline(x + 16, y + 66, w - 32, WM_COLOR_BORDER);

    fb_draw_string(x + 16, y + 80, "A hobby operating system", WM_COLOR_TEXT);
    fb_draw_string(x + 16, y + 100, "built from scratch with", WM_COLOR_TEXT);
    fb_draw_string(x + 16, y + 120, "bare metal C and x86-64 ASM.", WM_COLOR_TEXT);

    fb_draw_string(x + 16, y + 150, "Features:", WM_COLOR_ACCENT);
    fb_draw_string(x + 24, y + 170, "Window Manager", WM_COLOR_TEXT);
    fb_draw_string(x + 24, y + 190, "Mouse & Keyboard", WM_COLOR_TEXT);
    fb_draw_string(x + 24, y + 210, "Desktop Icons", WM_COLOR_TEXT);
    fb_draw_string(x + 24, y + 230, "Taskbar & Start Menu", WM_COLOR_TEXT);
    fb_draw_string(x + 24, y + 250, "Text Editor & Calculator", WM_COLOR_TEXT);
    fb_draw_string(x + 24, y + 270, "System Monitor", WM_COLOR_TEXT);

    fb_hline(x + 16, y + 296, w - 32, WM_COLOR_BORDER);
    fb_draw_string(x + 16, y + 308, "Made with bare metal C & ASM", WM_COLOR_TEXT_DIM);
}

#define EDITOR_MAX_LINES 64
#define EDITOR_LINE_LEN 80

typedef struct {
    char lines[EDITOR_MAX_LINES][EDITOR_LINE_LEN];
    int32_t line_count;
    int32_t cursor_line;
    int32_t cursor_col;
    int32_t scroll_y;
} editor_state_t;

static editor_state_t editor = {0};

static void editor_init(void) {
    for (int32_t i = 0; i < EDITOR_MAX_LINES; i++)
        for (int32_t j = 0; j < EDITOR_LINE_LEN; j++)
            editor.lines[i][j] = 0;
    editor.line_count = 1;
    editor.cursor_line = 0;
    editor.cursor_col = 0;
    editor.scroll_y = 0;

    const char *welcome = "Welcome to Qiki Text Editor!";
    int32_t k = 0;
    while (welcome[k] && k < EDITOR_LINE_LEN - 1) {
        editor.lines[0][k] = welcome[k];
        k++;
    }
    editor.lines[0][k] = 0;
}

static void editor_keypress(char c, uint8_t scancode) {
    if (c == '\b') {
        if (editor.cursor_col > 0) {
            for (int32_t i = editor.cursor_col - 1; i < EDITOR_LINE_LEN - 1; i++) {
                editor.lines[editor.cursor_line][i] = editor.lines[editor.cursor_line][i + 1];
            }
            editor.cursor_col--;
        } else if (editor.cursor_line > 0) {
            editor.cursor_col = 0;
            while (editor.lines[editor.cursor_line][editor.cursor_col]) editor.cursor_col++;
            int32_t prev_len = editor.cursor_col;
            for (int32_t i = 0; i < EDITOR_LINE_LEN - prev_len; i++) {
                editor.lines[editor.cursor_line - 1][prev_len + i] = editor.lines[editor.cursor_line][i];
            }
            editor.cursor_line--;
            editor.cursor_col = prev_len;
        }
        return;
    }
    if (c == '\n') {
        if (editor.line_count < EDITOR_MAX_LINES) {
            int32_t new_line = editor.line_count;
            int32_t remaining = EDITOR_LINE_LEN - editor.cursor_col - 1;
            if (remaining > 0) {
                for (int32_t i = 0; i < remaining; i++) {
                    editor.lines[new_line][i] = editor.lines[editor.cursor_line][editor.cursor_col + i];
                    editor.lines[editor.cursor_line][editor.cursor_col + i] = 0;
                }
            }
            editor.line_count++;
            editor.cursor_line++;
            editor.cursor_col = 0;
        }
        return;
    }
    if (scancode == 0x48) {
        if (editor.cursor_line > 0) editor.cursor_line--;
        return;
    }
    if (scancode == 0x50) {
        if (editor.cursor_line < editor.line_count - 1) editor.cursor_line++;
        return;
    }
    if (scancode == 0x4B) {
        if (editor.cursor_col > 0) editor.cursor_col--;
        return;
    }
    if (scancode == 0x4D) {
        if (editor.cursor_col < EDITOR_LINE_LEN - 1) editor.cursor_col++;
        return;
    }

    if (c >= 32 && c < 127 && editor.cursor_col < EDITOR_LINE_LEN - 1) {
        for (int32_t i = EDITOR_LINE_LEN - 1; i > editor.cursor_col; i--) {
            editor.lines[editor.cursor_line][i] = editor.lines[editor.cursor_line][i - 1];
        }
        editor.lines[editor.cursor_line][editor.cursor_col] = c;
        editor.cursor_col++;
    }
}

static void draw_editor_content(int32_t x, int32_t y, int32_t w, int32_t h) {
    fb_rect(x, y, w, h, 0x0D1117);

    int32_t line_height = 12;
    int32_t visible_lines = h / line_height;
    int32_t gutter_w = 36;

    if (editor.cursor_line - editor.scroll_y >= visible_lines)
        editor.scroll_y = editor.cursor_line - visible_lines + 1;
    if (editor.cursor_line < editor.scroll_y)
        editor.scroll_y = editor.cursor_line;

    for (int32_t i = 0; i < visible_lines && i < editor.line_count - editor.scroll_y; i++) {
        int32_t line_num = editor.scroll_y + i;
        int32_t ly = y + i * line_height;

        if (line_num == editor.cursor_line) {
            fb_rect(x, ly, w, line_height, 0x1A2232);
        }

        char num_str[6] = {0};
        int32_t n = line_num + 1;
        int32_t digits = 0, tmp = n;
        if (tmp == 0) { num_str[0] = '0'; digits = 1; }
        else { while (tmp > 0) { digits++; tmp /= 10; } }
        n = line_num + 1;
        for (int32_t d = digits - 1; d >= 0; d--) { num_str[digits - 1 - d] = '0' + (n % 10); n /= 10; }

        fb_draw_string(x + 4, ly + 2, num_str, fb_color(0x4A, 0x55, 0x65));

        fb_draw_string(x + gutter_w, ly + 2, editor.lines[line_num], 0xC9D1D9);

        if (line_num == editor.cursor_line) {
            int32_t cx2 = x + gutter_w + editor.cursor_col * 8;
            if ((timer_ticks % 30) < 15) {
                fb_rect(cx2, ly, 2, line_height, 0x58A6FF);
            }
        }
    }

    fb_rect(x, y + h - 20, w, 20, 0x161B22);
    char status[24] = "Ln 1  Col 1";
    status[3] = '0' + ((editor.cursor_line + 1) / 10);
    status[4] = '0' + ((editor.cursor_line + 1) % 10);
    status[8] = '0' + ((editor.cursor_col + 1) / 10);
    status[9] = '0' + ((editor.cursor_col + 1) % 10);
    fb_draw_string(x + 8, y + h - 16, status, fb_color(0x4A, 0x55, 0x65));
}

static int32_t calc_display = 0;
static int32_t calc_operand = 0;
static int8_t calc_operator = 0;
static uint8_t calc_new_number = 1;
static char calc_display_str[16] = "0";

static void calc_update_display(void) {
    int32_t val = calc_display;
    int32_t start = 0;
    if (val < 0) { calc_display_str[0] = '-'; val = -val; start = 1; }

    char tmp[12] = {0};
    int32_t idx = 0;
    if (val == 0) { tmp[0] = '0'; idx = 1; }
    else { while (val > 0) { tmp[idx++] = '0' + (val % 10); val /= 10; } }

    for (int32_t i = idx - 1; i >= 0; i--) {
        calc_display_str[start++] = tmp[i];
    }
    calc_display_str[start] = 0;
}

static void calc_keypress(char c, uint8_t scancode) {
    (void)scancode;
    if (c >= '0' && c <= '9') {
        if (calc_new_number) { calc_display = 0; calc_new_number = 0; }
        calc_display = calc_display * 10 + (c - '0');
        calc_update_display();
    } else if (c == '+' || c == '-' || c == '*' || c == '/') {
        calc_operand = calc_display;
        calc_operator = c;
        calc_new_number = 1;
    } else if (c == '=' || c == '\n') {
        if (calc_operator == '+') calc_display = calc_operand + calc_display;
        else if (calc_operator == '-') calc_display = calc_operand - calc_display;
        else if (calc_operator == '*') calc_display = calc_operand * calc_display;
        else if (calc_operator == '/' && calc_display != 0) calc_display = calc_operand / calc_display;
        calc_operator = 0;
        calc_new_number = 1;
        calc_update_display();
    } else if (c == 'c' || c == 'C') {
        calc_display = 0;
        calc_operand = 0;
        calc_operator = 0;
        calc_new_number = 1;
        calc_update_display();
    }
}

static void draw_calculator_content(int32_t x, int32_t y, int32_t w, int32_t h) {
    fb_rect(x, y, w, h, 0x1A1A2E);

    fb_rect(x + 8, y + 8, w - 16, 48, 0x0D1117);
    fb_rect_outline(x + 8, y + 8, w - 16, 48, fb_color(0x30, 0x36, 0x3F), 1);

    int32_t dw = fb_string_width(calc_display_str);
    fb_draw_string(x + w - 16 - dw, y + 22, calc_display_str, 0xE6EDF3);

    if (calc_operator) {
        fb_draw_char(x + 16, y + 22, calc_operator, 0x58A6FF);
    }

    const char *btn_labels[] = {
        "C", "(", ")", "/",
        "7", "8", "9", "*",
        "4", "5", "6", "-",
        "1", "2", "3", "+",
        "0", ".", "=", "="
    };
    uint32_t btn_colors[] = {
        0xEF4444, 0x4A5565, 0x4A5565, 0x3B82F6,
        0x2D3348, 0x2D3348, 0x2D3348, 0x3B82F6,
        0x2D3348, 0x2D3348, 0x2D3348, 0x3B82F6,
        0x2D3348, 0x2D3348, 0x2D3348, 0x3B82F6,
        0x2D3348, 0x2D3348, 0x22C55E, 0x22C55E
    };

    int32_t btn_w = (w - 24) / 4;
    int32_t btn_h = (h - 68) / 5;

    for (int32_t row = 0; row < 5; row++) {
        for (int32_t col = 0; col < 4; col++) {
            int32_t idx = row * 4 + col;
            int32_t bx = x + 8 + col * (btn_w + 2);
            int32_t by = y + 62 + row * (btn_h + 2);

            fb_rect_rounded(bx, by, btn_w, btn_h, 4, btn_colors[idx]);

            int32_t tw = fb_string_width(btn_labels[idx]);
            fb_draw_string(bx + btn_w / 2 - tw / 2, by + btn_h / 2 - 4, btn_labels[idx], 0xFFFFFF);
        }
    }
}

static void draw_sysmon_content(int32_t x, int32_t y, int32_t w, int32_t h) {
    fb_rect(x, y, w, h, WM_COLOR_WINDOW_BG2);

    fb_draw_string_scaled(x + 16, y + 12, "System Monitor", WM_COLOR_ACCENT, 2);

    fb_hline(x + 16, y + 38, w - 32, WM_COLOR_BORDER);

    fb_draw_string(x + 16, y + 50, "Uptime", WM_COLOR_TEXT_DIM);
    fb_draw_string(x + 16, y + 68, "Timer ticks:", WM_COLOR_TEXT);

    char tick_str[12] = {0};
    int32_t val = (int32_t)timer_ticks;
    int32_t idx = 0;
    if (val == 0) { tick_str[0] = '0'; idx = 1; }
    else { while (val > 0 && idx < 10) { tick_str[idx++] = '0' + (val % 10); val /= 10; } }
    for (int32_t i = 0; i < idx / 2; i++) { char t = tick_str[i]; tick_str[i] = tick_str[idx - 1 - i]; tick_str[idx - 1 - i] = t; }
    tick_str[idx] = 0;
    fb_draw_string(x + 120, y + 68, tick_str, 0x22C55E);

    fb_hline(x + 16, y + 90, w - 32, WM_COLOR_BORDER);

    fb_draw_string(x + 16, y + 100, "Memory", WM_COLOR_TEXT_DIM);

    fb_draw_string(x + 16, y + 120, "Total:", WM_COLOR_TEXT);
    fb_draw_string(x + 80, y + 120, "64 MB", 0x3B82F6);

    int32_t bar_x = x + 16;
    int32_t bar_y = y + 145;
    int32_t bar_w = w - 32;
    int32_t bar_h = 16;

    fb_rect_rounded(bar_x, bar_y, bar_w, bar_h, 4, 0x1A2332);
    int32_t fill = (bar_w * 30) / 100;
    fb_rect_rounded(bar_x, bar_y, fill, bar_h, 4, 0x3B82F6);

    fb_draw_string(x + 16, y + 170, "30% used", WM_COLOR_TEXT);

    fb_hline(x + 16, y + 195, w - 32, WM_COLOR_BORDER);

    fb_draw_string(x + 16, y + 205, "Display", WM_COLOR_TEXT_DIM);
    fb_draw_string(x + 16, y + 225, "Resolution:", WM_COLOR_TEXT);

    char res_str[20] = {0};
    int32_t rw = fb.width;
    int32_t ri = 0;
    if (rw == 0) { res_str[0] = '0'; ri = 1; }
    else { while (rw > 0 && ri < 10) { res_str[ri++] = '0' + (rw % 10); rw /= 10; } }
    for (int32_t i = 0; i < ri / 2; i++) { char t = res_str[i]; res_str[i] = res_str[ri - 1 - i]; res_str[ri - 1 - i] = t; }
    res_str[ri] = 'x';
    ri++;
    rw = fb.height;
    int32_t ri2 = ri;
    if (rw == 0) { res_str[ri2++] = '0'; }
    else { while (rw > 0 && ri2 < ri + 10) { res_str[ri2++] = '0' + (rw % 10); rw /= 10; } }
    for (int32_t i = ri; i < (ri + ri2) / 2; i++) { char t = res_str[i]; res_str[i] = res_str[ri2 - 1 - i]; res_str[ri2 - 1 - i] = t; }
    res_str[ri2] = 0;
    fb_draw_string(x + 112, y + 225, res_str, 0x22C55E);

    fb_draw_string(x + 16, y + 250, "Color depth:", WM_COLOR_TEXT);
    char bpp_str[4] = {0};
    int32_t bv = fb.bpp;
    int32_t bi = 0;
    if (bv == 0) { bpp_str[0] = '0'; bi = 1; }
    else { while (bv > 0 && bi < 3) { bpp_str[bi++] = '0' + (bv % 10); bv /= 10; } }
    for (int32_t i = 0; i < bi / 2; i++) { char t = bpp_str[i]; bpp_str[i] = bpp_str[bi - 1 - i]; bpp_str[bi - 1 - i] = t; }
    bpp_str[bi] = 0;
    fb_draw_string(x + 112, y + 250, bpp_str, 0x22C55E);
    fb_draw_string(x + 112 + bi * 8, y + 250, "bpp", 0x22C55E);
}

static int32_t fm_current_folder = 0;

typedef struct {
    const char *name;
    uint8_t is_dir;
    int32_t parent;
    uint32_t color;
} fm_entry_t;

static const fm_entry_t fm_fs[] = {
    {"Home",          1, -1, 0x3B82F6},
    {"Desktop",       1,  0, 0x22C55E},
    {"Documents",     1,  0, 0xF59E0B},
    {"Downloads",     1,  0, 0x8B5CF6},
    {"readme.txt",    0,  0, 0xE2E8F0},
    {"config.sys",    0,  0, 0xE2E8F0},
    {"Music",         1,  0, 0xEF4444},
    {"Pictures",      1,  0, 0x3B82F6},
    {"Desktop Notes", 0,  1, 0xE2E8F0},
    {"resume.doc",    0,  2, 0xE2E8F0},
    {"budget.txt",    0,  2, 0xE2E8F0},
    {"kernel.bin",    0,  3, 0x22C55E},
    {"archive.zip",   0,  3, 0xF59E0B},
    {"playlist.m3u",  0,  5, 0xEF4444},
    {"wallpaper.bmp", 0,  6, 0x3B82F6},
    {"screenshot.bmp",0,  6, 0x8B5CF6},
};
#define FM_FS_COUNT (sizeof(fm_fs) / sizeof(fm_fs[0]))

static int32_t fm_count_items(int32_t folder) {
    int32_t count = 0;
    for (int32_t i = 0; i < (int32_t)FM_FS_COUNT; i++)
        if (fm_fs[i].parent == folder) count++;
    return count;
}

static int32_t fm_get_item(int32_t folder, int32_t index) {
    int32_t count = 0;
    for (int32_t i = 0; i < (int32_t)FM_FS_COUNT; i++) {
        if (fm_fs[i].parent == folder) {
            if (count == index) return i;
            count++;
        }
    }
    return -1;
}

static void draw_files_content(int32_t x, int32_t y, int32_t w, int32_t h) {
    fb_rect(x, y, w, h, WM_COLOR_WINDOW_BG2);

    fb_rect(x, y, 140, h, 0x141C28);
    fb_rect(140, y, 1, h, WM_COLOR_BORDER);

    fb_draw_string(x + 8, y + 10, "Folders", WM_COLOR_TEXT_DIM);

    const char *folder_names[] = {"Home", "Desktop", "Documents", "Downloads"};
    uint32_t folder_colors[] = {0x3B82F6, 0x22C55E, 0xF59E0B, 0x8B5CF6};
    for (int32_t i = 0; i < 4; i++) {
        int32_t fy = y + 36 + i * 28;
        uint32_t bg = (fm_current_folder == i + 1) ? 0x2A4A6A : 0x1E2D42;
        fb_rect_rounded(x + 4, fy, 132, 24, 3, bg);
        fb_draw_char(x + 12, fy + 5, 'D', folder_colors[i]);
        fb_draw_string(x + 28, fy + 5, folder_names[i], WM_COLOR_TEXT);
    }

    int32_t content_x = x + 148;
    int32_t content_w = w - 156;

    fb_draw_string(content_x, y + 10, "Files", WM_COLOR_TEXT);

    int32_t item_count = fm_count_items(fm_current_folder);
    int32_t item_y = y + 36;

    for (int32_t i = 0; i < item_count && item_y < y + h - 20; i++) {
        int32_t idx = fm_get_item(fm_current_folder, i);
        if (idx < 0) continue;
        const fm_entry_t *ent = &fm_fs[idx];

        fb_rect_rounded(content_x, item_y, content_w, 24, 3, 0x1E2D42);

        if (ent->is_dir) {
            fb_draw_char(content_x + 8, item_y + 5, 'D', ent->color);
            fb_draw_string(content_x + 28, item_y + 5, ent->name, WM_COLOR_TEXT);
            fb_draw_char(content_x + content_w - 16, item_y + 5, '>', 0x4A5565);
        } else {
            fb_draw_char(content_x + 8, item_y + 5, 'F', ent->color);
            fb_draw_string(content_x + 28, item_y + 5, ent->name, WM_COLOR_TEXT);
        }
        item_y += 28;
    }

    if (item_count == 0) {
        fb_draw_string(content_x, item_y + 10, "Folder is empty", WM_COLOR_TEXT_DIM);
    }

    fb_rect(x, y + h - 18, w, 18, 0x161B22);
    char status[24];
    int32_t si = 0;
    if (item_count > 0 && item_count < 10) { status[si++] = '0' + item_count; }
    else if (item_count >= 10) { status[si++] = '0' + (item_count / 10); status[si++] = '0' + (item_count % 10); }
    const char *suf = " items";
    for (int32_t i = 0; i < 6; i++) status[si++] = suf[i];
    status[si] = 0;
    fb_draw_string(x + 8, y + h - 14, status, fb_color(0x4A, 0x55, 0x65));
}

static void files_click(int32_t cx, int32_t cy) {
    if (cx < 140) {
        for (int32_t i = 0; i < 4; i++) {
            int32_t fy = 36 + i * 28;
            if (cy >= fy && cy < fy + 24) {
                fm_current_folder = i + 1;
                return;
            }
        }
        return;
    }

    int32_t item_count = fm_count_items(fm_current_folder);
    for (int32_t i = 0; i < item_count; i++) {
        int32_t fy = 36 + i * 28;
        if (cy >= fy && cy < fy + 24 && cx >= 148) {
            int32_t idx = fm_get_item(fm_current_folder, i);
            if (idx >= 0 && fm_fs[idx].is_dir) {
                fm_current_folder = idx;
            }
            return;
        }
    }
}

static void open_files(void) {
    int32_t existing = wm_find_window_by_title("Files");
    if (existing >= 0) {
        wm_bring_to_front(existing);
        return;
    }
    fm_current_folder = 0;
    int32_t id = wm_create_window("Files", 300, 70, 500, 360, draw_files_content);
    if (id >= 0) {
        wm.windows[id].on_click = files_click;
        wm_add_taskbar_entry("Files", 'F', 0xF59E0B, id);
    }
}

static void open_about(void) {
    int32_t existing = wm_find_window_by_title("About Qiki OS");
    if (existing >= 0) {
        wm_bring_to_front(existing);
        return;
    }
    int32_t id = wm_create_window("About Qiki OS", 250, 80, 400, 380, draw_about_content);
    if (id >= 0) wm_add_taskbar_entry("About", 'A', 0x3B82F6, id);
}

static void open_text_editor(void) {
    int32_t existing = wm_find_window_by_title("Text Editor");
    if (existing >= 0) {
        wm_bring_to_front(existing);
        return;
    }
    editor_init();
    int32_t id = wm_create_window("Text Editor", 100, 40, 560, 420, draw_editor_content);
    if (id >= 0) {
        wm.windows[id].on_keypress = editor_keypress;
        wm_add_taskbar_entry("Editor", 'E', 0x58A6FF, id);
    }
}

static void calc_click(int32_t cx, int32_t cy) {
    int32_t w = 260;
    int32_t h = 360;
    int32_t btn_w = (w - 24) / 4;
    int32_t btn_h = (h - 68) / 5;
    int32_t content_y = WM_TITLEBAR_HEIGHT + 1;

    int32_t adj_cy = cy - content_y + 1;

    if (adj_cy < 62) return;

    int32_t col = (cx - 8) / (btn_w + 2);
    int32_t row = (adj_cy - 62) / (btn_h + 2);

    if (col < 0 || col >= 4 || row < 0 || row >= 5) return;

    int32_t idx = row * 4 + col;
    const char *btn_labels[] = {
        "C", "(", ")", "/",
        "7", "8", "9", "*",
        "4", "5", "6", "-",
        "1", "2", "3", "+",
        "0", ".", "=", "="
    };
    char c = btn_labels[idx][0];
    calc_keypress(c, 0);
}

static void open_calculator(void) {
    int32_t existing = wm_find_window_by_title("Calculator");
    if (existing >= 0) {
        wm_bring_to_front(existing);
        return;
    }
    calc_display = 0;
    calc_operand = 0;
    calc_operator = 0;
    calc_new_number = 1;
    calc_display_str[0] = '0';
    calc_display_str[1] = 0;
    int32_t id = wm_create_window("Calculator", 400, 100, 260, 360, draw_calculator_content);
    if (id >= 0) {
        wm.windows[id].on_keypress = calc_keypress;
        wm.windows[id].on_click = calc_click;
        wm_add_taskbar_entry("Calc", 'C', 0xF59E0B, id);
    }
}

static void open_sysmon(void) {
    int32_t existing = wm_find_window_by_title("System Monitor");
    if (existing >= 0) {
        wm_bring_to_front(existing);
        return;
    }
    int32_t id = wm_create_window("System Monitor", 180, 50, 380, 340, draw_sysmon_content);
    if (id >= 0) wm_add_taskbar_entry("SysMon", 'M', 0x22C55E, id);
}

static uint32_t settings_accent = 0x3B82F6;

static void draw_settings_content(int32_t x, int32_t y, int32_t w, int32_t h) {
    fb_rect(x, y, w, h, WM_COLOR_WINDOW_BG2);

    fb_rect(x, y, 120, h, 0x141C28);
    fb_rect(120, y, 1, h, WM_COLOR_BORDER);

    fb_draw_string(x + 8, y + 10, "System", WM_COLOR_TEXT);

    const char *tabs[] = {"Display", "Theme", "About"};
    for (int32_t i = 0; i < 3; i++) {
        int32_t ty2 = y + 30 + i * 28;
        fb_rect_rounded(x + 4, ty2, 112, 24, 3, 0x1E2D42);
        fb_draw_string(x + 12, ty2 + 6, tabs[i], WM_COLOR_TEXT);
    }

    int32_t cx = x + 128;
    int32_t cw = w - 136;

    fb_draw_string_scaled(cx, y + 10, "System Info", settings_accent, 2);
    fb_hline(cx, y + 36, cw, WM_COLOR_BORDER);

    fb_draw_string(cx, y + 48, "OS:", WM_COLOR_TEXT_DIM);
    fb_draw_string(cx + 60, y + 48, "Qiki OS 0.4", WM_COLOR_TEXT);

    fb_draw_string(cx, y + 68, "Kernel:", WM_COLOR_TEXT_DIM);
    fb_draw_string(cx + 60, y + 68, "x86_64 Bare Metal", WM_COLOR_TEXT);

    fb_draw_string(cx, y + 88, "Resolution:", WM_COLOR_TEXT_DIM);
    char res_str[20] = {0};
    int32_t rw = fb.width;
    int32_t ri = 0;
    if (rw == 0) { res_str[0] = '0'; ri = 1; }
    else { while (rw > 0 && ri < 10) { res_str[ri++] = '0' + (rw % 10); rw /= 10; } }
    for (int32_t i = 0; i < ri / 2; i++) { char t = res_str[i]; res_str[i] = res_str[ri - 1 - i]; res_str[ri - 1 - i] = t; }
    res_str[ri] = 'x';
    ri++;
    rw = fb.height;
    int32_t ri2 = ri;
    if (rw == 0) { res_str[ri2++] = '0'; }
    else { while (rw > 0 && ri2 < ri + 10) { res_str[ri2++] = '0' + (rw % 10); rw /= 10; } }
    for (int32_t i = ri; i < (ri + ri2) / 2; i++) { char t = res_str[i]; res_str[i] = res_str[ri2 - 1 - i]; res_str[ri2 - 1 - i] = t; }
    res_str[ri2] = 0;
    fb_draw_string(cx + 60, y + 88, res_str, WM_COLOR_TEXT);

    fb_draw_string(cx, y + 108, "Color:", WM_COLOR_TEXT_DIM);
    char bpp_str[8] = {0};
    int32_t bv = fb.bpp;
    int32_t bi = 0;
    if (bv == 0) { bpp_str[0] = '0'; bi = 1; }
    else { while (bv > 0 && bi < 6) { bpp_str[bi++] = '0' + (bv % 10); bv /= 10; } }
    for (int32_t i = 0; i < bi / 2; i++) { char t = bpp_str[i]; bpp_str[i] = bpp_str[bi - 1 - i]; bpp_str[bi - 1 - i] = t; }
    bpp_str[bi] = 'b';
    bpp_str[bi + 1] = 'p';
    bpp_str[bi + 2] = 'p';
    bpp_str[bi + 3] = 0;
    fb_draw_string(cx + 60, y + 108, bpp_str, WM_COLOR_TEXT);

    fb_hline(cx, y + 128, cw, WM_COLOR_BORDER);

    fb_draw_string(cx, y + 140, "Memory:", WM_COLOR_TEXT_DIM);
    fb_draw_string(cx + 60, y + 140, "64 MB", WM_COLOR_TEXT);

    int32_t bar_x = cx;
    int32_t bar_y = y + 162;
    int32_t bar_w2 = cw;
    int32_t bar_h2 = 14;
    fb_rect_rounded(bar_x, bar_y, bar_w2, bar_h2, 4, 0x1A2332);
    fb_rect_rounded(bar_x, bar_y, (bar_w2 * 15) / 100, bar_h2, 4, 0x3B82F6);
    fb_draw_string(cx, y + 184, "15% used", WM_COLOR_TEXT_DIM);

    fb_hline(cx, y + 204, cw, WM_COLOR_BORDER);

    fb_draw_string(cx, y + 216, "Accent Color:", WM_COLOR_TEXT_DIM);

    uint32_t colors[] = {0x3B82F6, 0x22C55E, 0xF59E0B, 0xEF4444, 0x8B5CF6};
    for (int32_t i = 0; i < 5; i++) {
        int32_t sx = cx + i * 36;
        int32_t sy = y + 238;
        fb_rect_rounded(sx, sy, 28, 28, 6, colors[i]);
        if (settings_accent == colors[i]) {
            fb_rect_outline(sx - 2, sy - 2, 32, 32, 0xFFFFFF, 2);
        }
    }
}

static void settings_click(int32_t cx, int32_t cy) {
    uint32_t colors[] = {0x3B82F6, 0x22C55E, 0xF59E0B, 0xEF4444, 0x8B5CF6};
    for (int32_t i = 0; i < 5; i++) {
        int32_t sx = i * 36;
        int32_t sy = WM_TITLEBAR_HEIGHT + 1 + 238;
        if (cx >= sx && cx < sx + 28 && cy >= sy && cy < sy + 28) {
            settings_accent = colors[i];
            return;
        }
    }
}

static void open_settings(void) {
    int32_t existing = wm_find_window_by_title("Settings");
    if (existing >= 0) {
        wm_bring_to_front(existing);
        return;
    }
    int32_t id = wm_create_window("Settings", 200, 90, 480, 380, draw_settings_content);
    if (id >= 0) {
        wm.windows[id].on_click = settings_click;
        wm_add_taskbar_entry("Settings", 'S', 0x8B5CF6, id);
    }
}

void kernel_main(uint64_t mb2_info) {
    serial_init();
    printk("Qiki OS v0.4 - Booting...\n");

    gdt_init();
    idt_init();
    pic_init();

    uint64_t total_mem = 64 * 1024 * 1024;
    mem_init(total_mem);
    heap_init();
    scheduler_init();

    fb_init(mb2_info);

    if (fb.addr == 0) {
        printk("No framebuffer available\n");
        while (1) __asm__ volatile ("hlt");
    }

    printk("GUI mode: %dx%d@%d\n", fb.width, fb.height, fb.bpp);
    printk("FB addr=0x%x pitch=%d backbuf=0x%x\n", fb.addr, fb.pitch, fb.backbuf);

    wm_init();

    wm_add_icon("About", 'A', 0x3B82F6, 24, 40, open_about);
    wm_add_icon("Terminal", '>', 0x22C55E, 24, 120, open_terminal);
    wm_add_icon("Editor", 'E', 0x58A6FF, 24, 200, open_text_editor);
    wm_add_icon("Files", 'F', 0xF59E0B, 24, 280, open_files);
    wm_add_icon("Calc", 'C', 0x22C55E, 24, 360, open_calculator);
    wm_add_icon("SysMon", 'M', 0x8B5CF6, 24, 440, open_sysmon);
    wm_add_icon("Settings", 'S', 0x8B5CF6, 24, 520, open_settings);

    wm_add_start_item("Text Editor", 'E', 0x58A6FF, open_text_editor);
    wm_add_start_item("Calculator", 'C', 0xF59E0B, open_calculator);
    wm_add_start_item("System Monitor", 'M', 0x22C55E, open_sysmon);
    wm_add_start_item("Files", 'F', 0x8B5CF6, open_files);
    wm_add_start_item("Terminal", '>', 0x22C55E, open_terminal);
    wm_add_start_item("Settings", 'S', 0x8B5CF6, open_settings);
    wm_add_start_item("About", 'A', 0x3B82F6, open_about);

    wm.ctx_on_refresh = refresh_desktop;
    wm.ctx_on_terminal = open_terminal;
    wm.ctx_on_about = open_about;

    mouse_init();

    printk("Desktop ready\n");

    wm.needs_redraw = 1;
    wm_redraw();

    __asm__ volatile ("sti");

    while (1) {
        __asm__ volatile ("hlt");
    }
}
