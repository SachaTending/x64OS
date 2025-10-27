#include <libc.h>

void *operator new(unsigned long size) {
    return malloc(size);
}

void *operator new[](unsigned long size) {
    return malloc(size);
}

void operator delete(void *ptr) {
    free(ptr);
}

void operator delete(void *ptr, unsigned long) {
    free(ptr);
}