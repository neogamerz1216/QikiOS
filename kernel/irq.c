#include <stdint.h>
#include "io.h"

extern void printk(const char *fmt, ...);

void pic_init() {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0x0);
    outb(0xA1, 0x0);
}

void isr_handler(uint64_t *regs) {
    uint64_t int_no = regs[15];
    printk("ISR %d triggered\n", int_no);
    if (int_no == 14) {
        uint64_t fault_addr;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(fault_addr));
        printk("Page fault at 0x%x\n", fault_addr);
    }
    if (int_no == 8 || int_no == 13 || int_no == 14) {
        while (1) __asm__ volatile ("hlt");
    }
}

void irq_handler(uint64_t *regs) {
    uint64_t irq_no = regs[15] - 32;
    
    if (irq_no == 0) {
        extern void timer_handler();
        timer_handler();
    } else if (irq_no == 1) {
        extern void keyboard_handler();
        keyboard_handler();
    } else if (irq_no == 12) {
        extern void mouse_handler();
        mouse_handler();
    }
    
    if (irq_no >= 8) {
        outb(0xA0, 0x20);
    }
    outb(0x20, 0x20);
}
