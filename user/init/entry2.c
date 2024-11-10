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
char buf[512];
int64_t fd;
void _start3() {
    puts("Hello, C World!\n");
    puts("For now, all applications should use syscall for syscalls.\n");
    fd = syscall(2, "/Makefile");
    if (fd < 0) {
        puts("Failed to open /Makefile, idk why.\n");
        for(;;);
    }
    puts("Successfully opened /Makefile\n");
    //char buf[512];
    syscall(0, fd, &buf, 510);
    puts("/Makefile contents:");puts((const char *)&buf);
    for(;;);
}
