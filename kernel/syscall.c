#include <stdint.h>

extern void printk(const char *fmt, ...);

void syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    (void)arg2;
    (void)arg3;
    
    switch (num) {
        case 0:
            printk((const char*)arg1);
            break;
        case 1:
            printk("Process exited with code %d\n", arg1);
            while (1) __asm__ volatile ("hlt");
            break;
        default:
            printk("Unknown syscall: %d\n", num);
    }
}
