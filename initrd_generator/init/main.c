#include <stdio.h>
#include <stdint.h>
const char *msg2 = "u know what? fuck mlibc.\n";

#define SYS_read      0
#define SYS_write     1
#define SYS_open      2
#define SYS_set_tls   3
#define SYS_mmap      4
#define SYS_exit      5
#define SUS_clock_get 6
#define SYS_tlb_set   7
#define SYS_getcwd    8
#define SYS_seek      9
#define SYS_ioctl     10
#define SYS_unmap     11
#define SYS_exec      12
#define SYS_fork      13
#define SYS_waitpid   14

void debug(const char *msg) {
    size_t ret;
    asm volatile ("syscall" : "=a" (ret) : "0" (512), "D" (msg) : "rcx", "r11", "memory");
}

void exec(const char *path) {
    size_t ret;
    asm volatile ("syscall" : "=a" (ret) : "0" (SYS_exec), "D" (path), "S" (0), "d" (0) : "rcx", "r11", "memory");
}

void write2(int fd, void *data, size_t size) {
    size_t ret;
    asm volatile ("syscall" : "=a" (ret) : "0" (SYS_write), "D" (fd), "S" (data), "d" (size) : "rcx", "r11", "memory");
}

void write_linux(int fd, void *data, size_t size) {
    size_t ret;
    asm volatile ("syscall" : "=a" (ret) : "0" (SYS_write), "D" (fd), "S" (data), "d" (size) : "rcx", "r11", "memory");
}

void exit_linux() {
    size_t ret;
    asm volatile ("syscall" : "=a" (ret) : "0" (60), "D" (-1) : "rcx", "r11", "memory");
}

int main();
#define MAGIC_TEXT "This program is only for x64OS. Please use x64OS to run this program.\n"
void __mlibc_entry(uintptr_t *entry_stack, int (*main_fn)(int argc, char *argv[], char *env[]));;
void pre_main(uint64_t stack, uint64_t fn) {
    //printf("mlibc workd!\n");
    write_linux(1, (void *)MAGIC_TEXT, sizeof(MAGIC_TEXT));
    exit_linux();
    debug("u know what? fuck mlibc.\n");
    debug("i'm gonna try to at least temporarly write code without libc\n");
    //exec("/busybox.static");
    debug("uhh, gonnal call __mlibc_entry bcz i can\n");
    __mlibc_entry((uintptr_t *)stack, (int (*)(int argc, char *argv[], char *env[]))main);
    while(1);
}
int main() {
    printf("omfg it works!\n");
    while(1);
    return 0;
}

void *__dso_handle;