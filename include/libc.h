#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

void *memset (void *__s, int __c, size_t __n);
int memcmp (const void *__s1, const void *__s2, size_t __n);
void memcpy(void *dst, const void *src, size_t n);
void * malloc(size_t size);
void free(void *mem);
void *realloc(void *ptr, size_t l);

size_t strlen(const char *str);
void strcpy(char *dst, const char *src);
void itoa(char *buf, unsigned long int n, int base);
int atoi(char * string);

int isspace(char c);

// base/ssfn.c
void ssfn_print(const wchar_t *txt);
void ssfn_cprint(const char *txt);
int ssfn_putc(uint32_t unicode); // from ssfn.h
void ssfn_putc2(uint32_t c);
#define lprint(txt) ssfn_print(L##txt)
void ssfn_set_fg(uint32_t fg);

// base/libc/print.cpp
void printf(const char *fmt, ...);
void vsprintf(char *buf, void(*putc)(char), const char *fmt, va_list lst);

void assert(const char *file, size_t lnum, const char *line, bool res);
void panic(const char *file, size_t lnum, const char *msg, ...);

#define ASSERT(a) assert(__FILE__, __LINE__, #a, a)
#define PANIC(a, ...) panic(__FILE__, __LINE__, a, __VA_ARGS__)

#ifdef __cplusplus
}
#endif