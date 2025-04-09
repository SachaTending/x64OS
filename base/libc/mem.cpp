#include <stddef.h>
#include <stdint.h>
#include <libc.h>
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
        if (n % 8 == 0) {
            //printf("memcpy(0x%lx, 0x%lx, %lu): doing accelerated uint64 memcpy.\n", dst, src, n);
            uint64_t *p1 = (uint64_t *)dst;
            uint64_t *p2 = (uint64_t *)src;
            for (size_t i=0;i<n/8;i++) {
                p1[i] = p2[i];
            }
            return;
        } else if (n % 4 == 0) {
            uint32_t *p1 = (uint32_t *)dst;
            uint32_t *p2 = (uint32_t *)src;
            for (size_t i=0;i<n/4;i++) {
                p1[i] = p2[i];
            }
            return;
        } else if (n % 2 == 0) {
            uint16_t *p1 = (uint16_t *)dst;
            uint16_t *p2 = (uint16_t *)src;
            for (size_t i=0;i<n/2;i++) {
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
}