#include <stddef.h>
#include <stdint.h>
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
        uint8_t *p1 = (uint8_t *)dst;
        uint8_t *p2 = (uint8_t *)src;
        for (size_t i=0;i<n;i++) {
            p1[i] = p2[i];
        }
    }
}