#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
extern long int syscall2 (long int __sysno, ...) __THROW;
void print_syscall(const char *txt, size_t len) {
    syscall2(1, 1, txt, len);
}

const char *txt = "Hello, C World!\n";

size_t strlen(const char *str) {
    size_t len = 0;
    while (*str++) {
        len++;
    }
    return len;
}

void puts(const char *txt) {
    print_syscall(txt, strlen(txt));
}

void _start3() {
    puts("Hello, C World!\n");
    puts("For now, all applications should use int 0x80 for syscalls.\n");
    for(;;);
}