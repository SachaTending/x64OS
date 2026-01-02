#include <stdint.h>
#include <stddef.h>
#include <libc.h>
#include <spinlock.h>
#include <krnl.hpp>

extern "C" {

    int memcmp (const void *__s1, const void *__s2, size_t __n) {
        uint8_t *p1 = (uint8_t *) __s1;
        uint8_t *p2 = (uint8_t *) __s2;
        for (size_t i=0;i<__n;i++) {
            if (p1[i] != p2[i]) {
                return 1;
            }
        }
        return 0;
    }
    void *memset (void *__s, int __c, size_t __n) {
        uint8_t *p1 = (uint8_t *)__s;
        for (size_t i=0;i<__n;i++) {
            p1[i] = __c;
        }
        return __s;
    }
    void memcpy(void *dst, const void *src, size_t n) {
        //asm volatile ("cli");
        if (n % 8 == 0) {
            //printf("memcpy(0x%lx, 0x%lx, %lu): doing accelerated uint64 memcpy.\n", dst, src, n);
            uint64_t *p1 = (uint64_t *)dst;
            uint64_t *p2 = (uint64_t *)src;
            for (size_t i=0;i<n/8;i++) {
                p1[i] = p2[i];
            }
            return;
        }
        uint8_t *p1 = (uint8_t *)dst;
        uint8_t *p2 = (uint8_t *)src;
        for (size_t i=0;i<n;i++) {
            p1[i] = p2[i];
        }
    }
    void* memmove(void* dstptr, const void* srcptr, size_t size) {
        unsigned char* dst = (unsigned char*) dstptr;
        const unsigned char* src = (const unsigned char*) srcptr;
        if (dst < src) {
            for (size_t i = 0; i < size; i++)
                dst[i] = src[i];
        } else {
            for (size_t i = size; i != 0; i--)
                dst[i-1] = src[i-1];
        }
        return dstptr;
    }
    size_t strlen(const char *str) {
        size_t o = 0;
        while (*str++)
        {
            o++;
        }
        return o;
    }
    void strcpy(char *dst, const char *src) {
        size_t len = strlen(src);
        for (size_t i=0;i<len;i++) {
            dst[i] = src[i];
        }
    }
    const char *strdup(const char *in) {
        size_t l = strlen(in)+1;
        const char *s = (const char *)malloc(l);
        memset((void *)s, 0, l);
        strcpy((char *)s, in);
        return s;
    }

    int strncmp(const char *s1, const char *s2, size_t n) {
        for (size_t i = 0; i < n; i++) {
            char c1 = s1[i], c2 = s2[i];
            if (c1 != c2) {
                return c1 - c2;
            }
            if (!c1) {
                return 0;
            }
        }

        return 0;
    }
    int strcmp(const char *s1, const char *s2) {
        for (size_t i = 0; ; i++) {
            char c1 = s1[i], c2 = s2[i];
            if (c1 != c2) {
                return c1 - c2;
            }
            if (!c1) {
                return 0;
            }
        }
    }
}

extern "C" {
    __attribute__((noinline)) void spinlock_acquire(spinlock_t *lock) {
        if (spinlock_test_and_acq(lock)) return;
        volatile size_t deadlock_counter = 0;
        for (;;) {
            if (spinlock_test_and_acq(lock)) {
                break;
            }
            if (++deadlock_counter >= 100000000) {
                goto deadlock;
            }
    #if defined (__x86_64__)
            asm volatile ("pause");
    #endif
        }
        lock->last_acquirer = __builtin_return_address(0);
        return;

    deadlock:
        PANIC("spinlock: Deadlock occurred at 0x%lx on lock 0x%lx whose last acquirer was 0x%lx", __builtin_return_address(0), lock, lock->last_acquirer);
    }

    __attribute__((noinline)) void spinlock_acquire_no_dead_check(spinlock_t *lock) {
        for (;;) {
            if (spinlock_test_and_acq(lock)) {
                break;
            }
    #if defined (__x86_64__)
            asm volatile ("pause");
    #endif
        }
        lock->last_acquirer = __builtin_return_address(0);
    }
}