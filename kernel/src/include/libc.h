#pragma once
#include <stddef.h>
#include <limine.h>

#ifdef __cplusplus
extern "C" {
#endif

int printf(const char *, ...);
void *memset (void *__s, int __c, size_t __n);
void memcpy(void *dst, const void *src, size_t n);
int memcmp (const void *__s1, const void *__s2, size_t __n);
size_t strlen(const char *str);
const char *strdup(const char *in);
int strncmp(const char *s1, const char *s2, size_t n);
int strcmp(const char *s1, const char *s2);
void *malloc(size_t);
void *realloc(void *ptr, size_t l);
void free(void *);
void *pmm_alloc(size_t pages);

extern volatile limine_hhdm_request hhdm_request;
#define VMM_HIGHER_HALF hhdm_request.response->offset

#ifdef __cplusplus
}
#endif

#define DIV_ROUNDUP(VALUE, DIV) ({ \
    typeof(VALUE) DIV_ROUNDUP_value = VALUE; \
    typeof(DIV) DIV_ROUNDUP_div = DIV; \
    (DIV_ROUNDUP_value + (DIV_ROUNDUP_div - 1)) / DIV_ROUNDUP_div; \
})

#define ALIGN_UP(VALUE, ALIGN) ({ \
    typeof(VALUE) ALIGN_UP_value = VALUE; \
    typeof(ALIGN) ALIGN_UP_align = ALIGN; \
    DIV_ROUNDUP(ALIGN_UP_value, ALIGN_UP_align) * ALIGN_UP_align; \
})

#define ALIGN_DOWN(VALUE, ALIGN) ({ \
    typeof(VALUE) ALIGN_DOWN_value = VALUE; \
    typeof(VALUE) ALIGN_DOWN_align = ALIGN; \
    (ALIGN_DOWN_value / ALIGN_DOWN_align) * ALIGN_DOWN_align; \
})

#define MIN(A, B) ({ \
    typeof(A) MIN_a = A; \
    typeof(B) MIN_b = B; \
    MIN_a < MIN_b ? MIN_a : MIN_b; \
})

#define MAX(A, B) ({ \
    typeof(A) MAX_a = A; \
    typeof(B) MAX_b = B; \
    MAX_a > MAX_b ? MAX_a : MAX_b; \
})

#define PAGE_SIZE 4096