#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

extern void serial_write(char c);

static void print_int(int64_t num, int base) {
    char buf[32];
    int i = 30;
    int neg = 0;
    
    if (num < 0) {
        neg = 1;
        num = -num;
    }
    
    buf[31] = '\0';
    if (num == 0) {
        buf[i--] = '0';
    } else {
        while (num > 0) {
            int digit = num % base;
            buf[i--] = (digit < 10) ? '0' + digit : 'A' + digit - 10;
            num /= base;
        }
    }
    if (neg) buf[i--] = '-';
    
    for (int j = i + 1; j < 31; j++) {
        serial_write(buf[j]);
    }
}

void printk(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    while (*fmt) {
        if (*fmt == '%' && *(fmt + 1)) {
            fmt++;
            switch (*fmt) {
                case 'd':
                    print_int(va_arg(args, int), 10);
                    break;
                case 'x':
                    print_int(va_arg(args, unsigned int), 16);
                    break;
                case 's': {
                    const char *s = va_arg(args, const char*);
                    while (*s) serial_write(*s++);
                    break;
                }
                case 'c':
                    serial_write(va_arg(args, int));
                    break;
                case '%':
                    serial_write('%');
                    break;
            }
        } else {
            serial_write(*fmt);
        }
        fmt++;
    }
    
    va_end(args);
}
