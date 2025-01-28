#include <libc.h>
#include <printf/printf.h>
void stacktrace(uintptr_t *s);
__attribute__((noreturn)) void panic(const char *file, size_t lnum, const char *msg, ...) {
    asm volatile("cli");
    printf("KERNEL PANIC. At %s:%u\nReason: ", file, lnum);
    va_list lst;
    va_start(lst, msg);
    vprintf_(msg, lst);
    va_end(lst);
    putchar_('\n');
    stacktrace(0);
    printf("\nSystem halted.\n");
    asm volatile("1: hlt;jmp 1");
}

__attribute__((noreturn)) void assert(const char *file, size_t lnum, const char *line, bool res) {
    if (!res) {
        PANIC("ASSERTATION FAULT. At %s:%u: %s", file, lnum, line);
    }
}