#define _GNU_SOURCE
#include <unistd.h>
//#include <sys/syscall.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "xmp.h"
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
char *buf;
int64_t fd;
#define S 80*1024
#define JUNK_OFF 1024
#define SAMP_RATE 8000
char b2[S] = {0};
extern char _binary_rain_eater_xm_start;
extern char _binary_rain_eater_xm_end;
void tracker_test() {
    puts("Test 2, playing audio.");
    printf("libxmp version: %s\n", xmp_version);
    xmp_context c;
    struct xmp_frame_info mi;
    c = xmp_create_context();
    long size = (u_int64_t)&_binary_rain_eater_xm_end - (u_int64_t)&_binary_rain_eater_xm_start;
    printf("0x%lx, %ld\n", &_binary_rain_eater_xm_start, size);
    printf("created xmp context.\n");
    //int ret = xmp_load_module(c, "/rain_eater.xm");
    int ret = xmp_load_module_from_memory(c, (const void *)&_binary_rain_eater_xm_start, size);
    if (ret != 0) {
        printf("oh no, failed to load module, ret: %d.\n", ret);
    }
    printf("loaded successfully.\n");
    ret = xmp_start_player(c, SAMP_RATE, 0);
    if (ret != 0) {
        printf("oh no, failed to start player, ret: %d.\n", ret);
    }
    //close(fd);
    fd = open("/dev/dsp", O_WRONLY);
    //int fd2 = open("/rain_eater.pcm", O_RDONLY);
    ioctl(fd, 1, SAMP_RATE);
    //while (xmp_play_frame(c) == 0) {
        //read(fd2, (void *)buf+JUNK_OFF, S-JUNK_OFF);
        //xmp_get_frame_info(c, &mi);
        //printf("frame: 0x%lx %d\n", mi.buffer, mi.buffer_size);
        //if (mi.loop_count > 0)    /* exit before looping */
        //    break;
        //write(fd, (void *)mi.buffer, mi.buffer_size);
    //}
}
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
    //buf = (char *)malloc(S);
    memset(b2, 0, S);
    buf = b2;
    //printf("argc: %d\n", argc);
    for (int i=0;i<argc;i++) {
        printf("argv[i]=%s\n", argv[i]);
    }
    //syscall2(0, fd, buf, 510);
    //puts("/Makefile contents:");
    //for (int i=0;i<510;i++) {
    //    putchar(buf[i]);
    //}
    execl("/dash", "/dash   ", "tcc_test.c", NULL);
    for(;;);
}
