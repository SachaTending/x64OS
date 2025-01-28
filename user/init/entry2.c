#define _GNU_SOURCE
#include <unistd.h>
//#include <sys/syscall.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
extern long int syscall2 (long int __sysno, ...);
void print_syscall(const char *txt, size_t len) {
    syscall2(1, 0, txt, len);
}

const char *txt = "Hello, C World!\n";

int printf(const char *a, ...);
int puts2(const char *txt) {
    //printf(txt);
    print_syscall(txt, strlen(txt));
}
volatile char *buf;
int64_t fd;
#define S 4096
int main(int argc, char **argv) {
    //puts("Hello, C World!\n");
    ////puts("For now, all applications should use syscall for syscalls.\n"); early port of mlibc is avaible
    //fd = open("/Makefile", O_RDONLY);
    //printf("opened file, fd %d\n", fd);
    //if (fd < 0) {
    //    puts("Failed to open /Makefile, idk why.\n");
    //    for(;;);
    //}
    //puts("Successfully opened /Makefile\n");
    ////char buf[512];
    buf = (char *)malloc(S);
    //syscall2(0, fd, buf, 510);
    //puts("/Makefile contents:");
    //for (int i=0;i<510;i++) {
    //    putchar(buf[i]);
    //}
    puts("Test 2, playing audio.");
    //close(fd);
    fd = open("/dev/dsp", O_WRONLY);
    int fd2 = open("/rain_eater.pcm", O_RDONLY);
    while (1) {
        read(fd2, (void *)buf, S);
        write(fd, (void *)buf, S);
    }
    for(;;);
}
