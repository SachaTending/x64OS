#include <libc.h>

void *operator new(unsigned long size) {
    void *ret = malloc(size);
    memset(ret, 0, size);
    return ret;
}

void *operator new[](unsigned long size) {
    void *ret = malloc(size);
    memset(ret, 0, size);
    return ret;
}

void operator delete(void *ptr) {
    free(ptr);
}

void operator delete(void *ptr, unsigned long) {
    free(ptr);
}