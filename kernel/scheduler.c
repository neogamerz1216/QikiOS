#include <stdint.h>
#include "window_manager.h"

extern void printk(const char *fmt, ...);

uint64_t timer_ticks = 0;

void timer_handler() {
    timer_ticks++;
    if (wm.needs_redraw) {
        wm_redraw();
    }
}

void scheduler_init() {
    printk("Scheduler initialized\n");
}
